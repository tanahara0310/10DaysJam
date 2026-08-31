#include "pch.h"
#include "Text/MsdfFont.h"

#include "Text/DirectWriteFontFace.h"
#include "Text/MsdfFontBaker.h"
#include "Text/MsdfFontCache.h"
#include "Text/TextEncoding.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RHI/Descriptor/DescriptorAllocator.h"
#include "Graphics/Texture/Gpu/TextureGpuUploader.h"
#include "Utility/Logger/Logger.h"

#include <externals/DirectXTex/DirectXTex.h>

#include <cstring>
#include <memory>

namespace CoreEngine
{
    MsdfFont::~MsdfFont()
    {
        if (graphicsCore_ && atlasHandle_.IsValid()) {
            if (auto* allocator = graphicsCore_->GetDescriptorAllocator()) {
                allocator->Free(atlasHandle_);
            }
        }
    }

    bool MsdfFont::Build(GraphicsCore* graphicsCore, const MsdfFontDesc& desc)
    {
        if (!graphicsCore) { return false; }
        graphicsCore_ = graphicsCore;

        // ── ①フォールバック列を開く ────────────────────────────
        // 文字ごとに先頭から探すので、開けたものは全て残す
        std::vector<std::unique_ptr<DirectWriteFontFace>> faces;
        fontChainNames_.clear();

        if (!desc.filePath.empty()) {
            auto face = std::make_unique<DirectWriteFontFace>();
            if (face->LoadFromFile(desc.filePath, desc.faceIndex)) {
                fontChainNames_.push_back(face->GetDisplayName());
                faces.push_back(std::move(face));
            }
        }

        for (const std::wstring& familyName : desc.systemFamilyNames) {
            auto face = std::make_unique<DirectWriteFontFace>();
            if (face->LoadFromSystem(familyName)) {
                fontChainNames_.push_back(familyName);
                faces.push_back(std::move(face));
            }
        }

        if (faces.empty()) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFont: 指定されたフォントを 1 つも開けませんでした");
            return false;
        }

        // メトリクスは先頭のフォントのものを使う
        resolvedFontName_ = fontChainNames_.front();

        std::vector<const DirectWriteFontFace*> faceChain;
        faceChain.reserve(faces.size());
        for (const auto& face : faces) {
            faceChain.push_back(face.get());
        }

        // ── ②焼く文字を集める ────────────────────────────────────
        std::vector<char32_t> codePoints = Utf8ToUtf32(desc.charsetUtf8);
        if (desc.includeAscii) {
            for (char32_t cp = U' '; cp <= U'~'; ++cp) {
                codePoints.push_back(cp);
            }
        }
        // 改行はグリフを持たないのでアトラスへ入れない（レイアウト側で処理する）
        std::erase_if(codePoints, [](char32_t cp) {
            return cp == U'\n' || cp == U'\r' || cp == U'\t';
            });

        if (codePoints.empty()) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFont: 焼く文字が指定されていません");
            return false;
        }

        // ── ③MSDF アトラスを用意する（キャッシュ優先）────────────
        // ベイクは重い（Release で 152 グリフ 0.66 秒、最適化なしの Debug では 15 秒）。
        // 指定・フォント・文字集合が同じなら前回の結果をそのまま使う
        MsdfBakeResult bake{};
        bool fromCache = false;
        std::filesystem::path cachePath;

        if (desc.useDiskCache) {
            const uint64_t cacheKey = MsdfFontCache::ComputeKey(desc, fontChainNames_);
            cachePath = MsdfFontCache::MakePath(desc.cacheDirectory, cacheKey);
            fromCache = MsdfFontCache::TryLoad(cachePath, bake);
        }

        if (fromCache) {
            Logger::GetInstance().Logf(LogLevel::Info, LogCategory::Resource,
                "MSDF アトラスをキャッシュから読み込みました: {} ({} グリフ)",
                Logger::GetInstance().PathToUtf8(cachePath), bake.glyphs.size());
        } else {
            bake = MsdfFontBaker::Bake(faceChain, codePoints, desc.bake);
            if (!bake.success) {
                return false;
            }
            if (desc.useDiskCache && MsdfFontCache::Save(cachePath, bake)) {
                Logger::GetInstance().Logf(LogLevel::Info, LogCategory::Resource,
                    "MSDF アトラスをキャッシュへ保存しました: {}",
                    Logger::GetInstance().PathToUtf8(cachePath));
            }
        }

        // 目視確認用の PNG は、焼き直したときと出力が消えているときだけ書く
        // （毎起動書くと数十 ms を無駄に払う）
        if (!desc.debugAtlasDumpPath.empty()) {
            std::error_code ec;
            if (!fromCache || !std::filesystem::exists(desc.debugAtlasDumpPath, ec)) {
                MsdfFontBaker::SaveAtlasPng(bake, desc.debugAtlasDumpPath);
            }
        }

        glyphs_ = bake.glyphs;
        notdefGlyph_ = bake.notdefGlyph;
        metrics_ = bake.metrics;
        pxRange_ = desc.bake.pxRange;
        atlasSize_ = {
            static_cast<float>(bake.atlasWidth),
            static_cast<float>(bake.atlasHeight)
        };

        // ── ④GPU へ転送 ────────────────────────────────────────
        // ミップ 1 枚・非圧縮・R8G8B8A8_UNORM（＝リニア）で固定する。
        // _SRGB にするとサンプル時にガンマ変換が入り、距離値が歪んで輪郭がずれる。
        DirectX::ScratchImage image;
        HRESULT hr = image.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            static_cast<size_t>(bake.atlasWidth),
            static_cast<size_t>(bake.atlasHeight),
            /*arraySize*/ 1,
            /*mipLevels*/ 1);
        if (FAILED(hr)) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFont: アトラス用イメージの確保に失敗しました (HRESULT: 0x{:08X})",
                static_cast<unsigned int>(hr));
            return false;
        }

        const DirectX::Image* destination = image.GetImage(0, 0, 0);
        const size_t sourcePitch = static_cast<size_t>(bake.atlasWidth) * 4;
        for (int y = 0; y < bake.atlasHeight; ++y) {
            std::memcpy(
                destination->pixels + destination->rowPitch * y,
                bake.pixels.data() + sourcePitch * y,
                sourcePitch);
        }

        auto uploaded = TextureGpuUploader::UploadAndCreateSrv(
            graphicsCore, image, "MsdfFontAtlas");

        atlasTexture_ = uploaded.texture;
        atlasHandle_ = uploaded.descriptor;

        if (!IsValid()) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFont: アトラスの GPU 転送に失敗しました");
            return false;
        }

        return true;
    }

    const MsdfGlyph* MsdfFont::FindGlyph(char32_t codePoint) const
    {
        const auto it = glyphs_.find(codePoint);
        return (it != glyphs_.end()) ? &it->second : nullptr;
    }

    const MsdfGlyph& MsdfFont::ResolveGlyph(char32_t codePoint) const
    {
        const auto it = glyphs_.find(codePoint);
        return (it != glyphs_.end()) ? it->second : notdefGlyph_;
    }
}

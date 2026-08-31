#include "pch.h"
#include "Text/MsdfFont.h"

#include "Text/DirectWriteFontFace.h"
#include "Text/MsdfFontBaker.h"
#include "Text/TextEncoding.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RHI/Descriptor/DescriptorAllocator.h"
#include "Graphics/Texture/Gpu/TextureGpuUploader.h"
#include "Utility/Logger/Logger.h"

#include <externals/DirectXTex/DirectXTex.h>

#include <cstring>

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

        // ── ①フォントを開く ──────────────────────────────────────
        DirectWriteFontFace face;
        if (!desc.filePath.empty()) {
            if (!face.LoadFromFile(desc.filePath, desc.faceIndex)) {
                return false;
            }
            resolvedFontName_ = desc.filePath;
        } else {
            resolvedFontName_ = face.LoadFromSystemPreferred(desc.systemFamilyNames);
            if (resolvedFontName_.empty()) {
                Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                    "MsdfFont: 候補のシステムフォントが 1 つも見つかりませんでした");
                return false;
            }
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

        // ── ③MSDF アトラスを生成 ────────────────────────────────
        const MsdfBakeResult bake = MsdfFontBaker::Bake(face, codePoints, desc.bake);
        if (!bake.success) {
            return false;
        }

        if (!desc.debugAtlasDumpPath.empty()) {
            MsdfFontBaker::SaveAtlasPng(bake, desc.debugAtlasDumpPath);
        }

        glyphs_ = bake.glyphs;
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
}

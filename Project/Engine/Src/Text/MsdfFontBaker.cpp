#include "pch.h"
#include "Text/MsdfFontBaker.h"

#include "Text/DirectWriteFontFace.h"
#include "Utility/Logger/Logger.h"

#include <Externals/msdfgen/msdfgen.h>
#include <externals/DirectXTex/DirectXTex.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace CoreEngine
{
    namespace
    {
        /// msdfgen 標準のコーナー判定角（ラジアン）。
        /// これより鋭い折れをコーナーとみなしてチャンネルを切り替える
        constexpr double kEdgeColoringAngleThreshold = 3.0;

        /// @brief 焼く前に用意しておくグリフ 1 件分の情報
        struct PendingGlyph
        {
            char32_t codePoint = 0;
            uint16_t glyphIndex = 0;
            float advance = 0.0f;

            bool hasOutline = false;

            /// 距離場のマージンを含めた em 境界（Y 上正）
            double left = 0.0, bottom = 0.0, right = 0.0, top = 0.0;
            /// アトラス上で占めるピクセルサイズ
            int width = 0, height = 0;
            /// アトラス上の配置（左上）
            int atlasX = 0, atlasY = 0;
        };

        /// @brief 高さ降順のシェルフ（棚）パッキング
        /// @details 同じ高さのグリフが横一列に並ぶので、単純なわりに隙間が少ない。
        ///          動的アトラスへ移行する際もこの棚構造がそのまま使える。
        /// @return 全て収まったら true
        bool ShelfPack(std::vector<PendingGlyph>& glyphs,
            const std::vector<size_t>& order,
            const MsdfBakeSettings& settings,
            int& outDropped)
        {
            int cursorX = settings.padding;
            int cursorY = settings.padding;
            int shelfHeight = 0;
            outDropped = 0;
            bool allFit = true;

            for (size_t index : order) {
                PendingGlyph& glyph = glyphs[index];
                if (!glyph.hasOutline) { continue; }

                // 現在の棚に入らなければ次の棚へ送る
                if (cursorX + glyph.width + settings.padding > settings.atlasWidth) {
                    cursorX = settings.padding;
                    cursorY += shelfHeight + settings.padding;
                    shelfHeight = 0;
                }

                if (cursorY + glyph.height + settings.padding > settings.atlasHeight) {
                    // アトラスが尽きた。以降のグリフは絵を持たない扱いにする
                    // （advance だけ残るので、レイアウトは崩れず字だけが消える）
                    glyph.hasOutline = false;
                    ++outDropped;
                    allFit = false;
                    continue;
                }

                glyph.atlasX = cursorX;
                glyph.atlasY = cursorY;

                cursorX += glyph.width + settings.padding;
                shelfHeight = (std::max)(shelfHeight, glyph.height);
            }

            return allFit;
        }
    } // namespace

    // ──────────────────────────────────────────────────────────────

    MsdfBakeResult MsdfFontBaker::Bake(
        const DirectWriteFontFace& face,
        const std::vector<char32_t>& codePoints,
        const MsdfBakeSettings& settings)
    {
        MsdfBakeResult result{};
        result.settings = settings;

        if (!face.IsValid()) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFontBaker: フォントが読み込まれていません");
            return result;
        }
        if (settings.glyphPixelSize <= 0 || settings.atlasWidth <= 0 || settings.atlasHeight <= 0) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFontBaker: アトラス設定が不正です");
            return result;
        }

        const auto startTime = std::chrono::steady_clock::now();

        const double pixelsPerEm = static_cast<double>(settings.glyphPixelSize);
        // 距離場のマージン。輪郭の外側 pxRange/2 px まで距離を持たせる
        const double halfRangeEm = 0.5 * settings.pxRange / pixelsPerEm;

        // ── 重複を除いた文字集合を作る ────────────────────────────
        std::vector<char32_t> uniqueCodePoints = codePoints;
        std::sort(uniqueCodePoints.begin(), uniqueCodePoints.end());
        uniqueCodePoints.erase(
            std::unique(uniqueCodePoints.begin(), uniqueCodePoints.end()),
            uniqueCodePoints.end());

        // ── ①アウトラインを取り出して大きさを測る ────────────────
        std::vector<PendingGlyph> pending;
        std::vector<msdfgen::Shape> shapes;
        pending.reserve(uniqueCodePoints.size());
        shapes.reserve(uniqueCodePoints.size());

        for (char32_t codePoint : uniqueCodePoints) {
            const uint16_t glyphIndex = face.GetGlyphIndex(codePoint);
            if (glyphIndex == 0 && codePoint != U'\0') {
                // フォントに収録が無い文字。豆腐を出さずに黙って捨てる
                // （実運用ではここでフォントフォールバックへ回す）
                Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                    "MsdfFontBaker: フォントに収録されていない文字を飛ばしました (U+{:04X})",
                    static_cast<uint32_t>(codePoint));
                continue;
            }

            msdfgen::Shape shape;
            if (!face.BuildShape(glyphIndex, shape)) {
                continue;
            }

            PendingGlyph glyph{};
            glyph.codePoint = codePoint;
            glyph.glyphIndex = glyphIndex;
            glyph.advance = face.GetAdvance(glyphIndex);

            if (shape.contours.empty()) {
                // 空白文字。絵は無いが advance は要る
                glyph.hasOutline = false;
                pending.push_back(glyph);
                shapes.emplace_back();
                ++result.blankGlyphCount;
                continue;
            }

            if (!shape.validate()) {
                Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                    "MsdfFontBaker: 輪郭が不正なので飛ばしました (U+{:04X})",
                    static_cast<uint32_t>(codePoint));
                continue;
            }
            shape.normalize();

            const msdfgen::Shape::Bounds bounds = shape.getBounds();

            // 距離場のマージンぶん外側へ広げる
            glyph.left = bounds.l - halfRangeEm;
            glyph.bottom = bounds.b - halfRangeEm;

            glyph.width = static_cast<int>(
                std::ceil((bounds.r + halfRangeEm - glyph.left) * pixelsPerEm));
            glyph.height = static_cast<int>(
                std::ceil((bounds.t + halfRangeEm - glyph.bottom) * pixelsPerEm));
            glyph.width = (std::max)(glyph.width, 1);
            glyph.height = (std::max)(glyph.height, 1);

            // 整数化した幅・高さに合わせて右上端を取り直す。
            // こうしておくと「plane 境界 ↔ UV 矩形」が誤差なく 1 対 1 で対応する
            glyph.right = glyph.left + glyph.width / pixelsPerEm;
            glyph.top = glyph.bottom + glyph.height / pixelsPerEm;

            glyph.hasOutline = true;

            pending.push_back(glyph);
            shapes.push_back(std::move(shape));
        }

        if (pending.empty()) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfFontBaker: 焼けるグリフが 1 つもありませんでした");
            return result;
        }

        // ── ②アトラスへ配置（高さ降順のシェルフパッキング）────────
        std::vector<size_t> order(pending.size());
        for (size_t i = 0; i < order.size(); ++i) { order[i] = i; }
        std::sort(order.begin(), order.end(), [&pending](size_t a, size_t b) {
            return pending[a].height > pending[b].height;
            });

        int dropped = 0;
        if (!ShelfPack(pending, order, settings, dropped)) {
            Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                "MsdfFontBaker: アトラス {}x{} に収まらなかったグリフが {} 件あります。"
                "アトラスを広げるか glyphPixelSize を下げてください",
                settings.atlasWidth, settings.atlasHeight, dropped);
        }
        result.droppedGlyphCount = dropped;

        // ── ③距離場を焼いてアトラスへ書き込む ────────────────────
        result.atlasWidth = settings.atlasWidth;
        result.atlasHeight = settings.atlasHeight;
        result.pixels.assign(
            static_cast<size_t>(settings.atlasWidth) * settings.atlasHeight * 4, 0);

        const msdfgen::Range distanceRange(settings.pxRange / pixelsPerEm);
        const msdfgen::MSDFGeneratorConfig generatorConfig; // 既定でエラー訂正が有効

        std::vector<float> scratch;

        for (size_t i = 0; i < pending.size(); ++i) {
            PendingGlyph& glyph = pending[i];

            MsdfGlyph out{};
            out.advance = glyph.advance;
            out.hasBitmap = glyph.hasOutline;

            if (glyph.hasOutline) {
                msdfgen::Shape& shape = shapes[i];

                // コーナーで輪郭を切り、隣り合う辺が 1 チャンネルだけ共有するように
                // RGB を割り当てる。MSDF が角を保てるのはこの彩色があるからで、
                // generateMTSDF より先に必ず通す必要がある
                msdfgen::edgeColoringSimple(shape, kEdgeColoringAngleThreshold);

                const msdfgen::Projection projection(
                    msdfgen::Vector2(pixelsPerEm, pixelsPerEm),
                    msdfgen::Vector2(-glyph.left, -glyph.bottom));

                const size_t floatCount =
                    static_cast<size_t>(glyph.width) * glyph.height * 4;
                scratch.assign(floatCount, 0.0f);

                // 既定の Y 上正で焼く（メモリ上の行 0 ＝ グリフの下端）
                msdfgen::BitmapRef<float, 4> bitmap(scratch.data(), glyph.width, glyph.height);
                msdfgen::generateMTSDF(bitmap, shape, projection, distanceRange, generatorConfig);

                // アトラスは top-down なので、行を反転しながら書き込む
                for (int y = 0; y < glyph.height; ++y) {
                    const int srcRow = glyph.height - 1 - y;
                    const size_t dstOffset =
                        (static_cast<size_t>(glyph.atlasY + y) * settings.atlasWidth
                            + glyph.atlasX) * 4;

                    for (int x = 0; x < glyph.width; ++x) {
                        const float* src = bitmap(x, srcRow);
                        uint8_t* dst = result.pixels.data() + dstOffset + static_cast<size_t>(x) * 4;
                        dst[0] = msdfgen::pixelFloatToByte(src[0]);
                        dst[1] = msdfgen::pixelFloatToByte(src[1]);
                        dst[2] = msdfgen::pixelFloatToByte(src[2]);
                        dst[3] = msdfgen::pixelFloatToByte(src[3]);
                    }
                }

                out.planeLeft = static_cast<float>(glyph.left);
                out.planeBottom = static_cast<float>(glyph.bottom);
                out.planeRight = static_cast<float>(glyph.right);
                out.planeTop = static_cast<float>(glyph.top);

                out.uvLeft = static_cast<float>(glyph.atlasX) / settings.atlasWidth;
                out.uvTop = static_cast<float>(glyph.atlasY) / settings.atlasHeight;
                out.uvRight = static_cast<float>(glyph.atlasX + glyph.width) / settings.atlasWidth;
                out.uvBottom = static_cast<float>(glyph.atlasY + glyph.height) / settings.atlasHeight;

                ++result.bakedGlyphCount;
            }

            result.glyphs[glyph.codePoint] = out;
        }

        result.metrics = face.GetMetrics();
        result.success = true;
        result.bakeSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - startTime).count();

        Logger::GetInstance().Logf(LogLevel::Info, LogCategory::Resource,
            "MSDF アトラスを生成しました: {}x{} / 描画グリフ {} 件・空白 {} 件 / "
            "{}px 焼き・pxRange {} / {:.2f} 秒",
            result.atlasWidth, result.atlasHeight,
            result.bakedGlyphCount, result.blankGlyphCount,
            settings.glyphPixelSize, settings.pxRange, result.bakeSeconds);

        return result;
    }

    bool MsdfFontBaker::SaveAtlasPng(const MsdfBakeResult& result, const std::filesystem::path& outPath)
    {
        if (!result.success || result.pixels.empty()) {
            return false;
        }

        // アルファには真の SDF が入っているため、そのまま書くと
        // ビューアで半透明になって MSDF の色が読めない。
        // 目視確認が目的なので不透明に潰した複製を書き出す。
        std::vector<uint8_t> opaque = result.pixels;
        for (size_t i = 3; i < opaque.size(); i += 4) {
            opaque[i] = 0xFF;
        }

        DirectX::Image image{};
        image.width = static_cast<size_t>(result.atlasWidth);
        image.height = static_cast<size_t>(result.atlasHeight);
        image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        image.rowPitch = static_cast<size_t>(result.atlasWidth) * 4;
        image.slicePitch = opaque.size();
        image.pixels = opaque.data();

        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);

        const HRESULT hr = DirectX::SaveToWICFile(
            image,
            DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
            outPath.c_str());

        if (FAILED(hr)) {
            Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                "MSDF アトラスの PNG 出力に失敗しました: {} (HRESULT: 0x{:08X})",
                Logger::GetInstance().PathToUtf8(outPath),
                static_cast<unsigned int>(hr));
            return false;
        }

        Logger::GetInstance().Logf(LogLevel::Info, LogCategory::Resource,
            "MSDF アトラスを書き出しました（目視確認用）: {}",
            Logger::GetInstance().PathToUtf8(outPath));
        return true;
    }
}

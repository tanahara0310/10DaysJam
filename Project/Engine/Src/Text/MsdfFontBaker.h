#pragma once

#include "Text/MsdfFontTypes.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace CoreEngine
{
    class DirectWriteFontFace;

    /// @brief アトラス生成の結果
    struct MsdfBakeResult
    {
        bool success = false;

        int atlasWidth = 0;
        int atlasHeight = 0;

        /// @brief アトラス画素（RGBA8・**top-down**・非圧縮）
        /// @details RGB に MSDF、A に真の SDF（MTSDF）が入る。
        ///          描画は median(rgb) だけで足りるが、A を持っておくと
        ///          縁取り・グロー・影を後から正確に足せる。
        std::vector<uint8_t> pixels;

        std::unordered_map<char32_t, MsdfGlyph> glyphs;
        MsdfFontMetrics metrics{};
        MsdfBakeSettings settings{};

        int bakedGlyphCount = 0;   ///< アトラスに絵を置いたグリフ数
        int blankGlyphCount = 0;   ///< 空白など輪郭を持たなかったグリフ数
        int droppedGlyphCount = 0; ///< アトラスが足りずに入らなかったグリフ数
        double bakeSeconds = 0.0;
    };

    /// @brief DirectWrite のアウトラインを msdfgen で MSDF アトラスへ焼く
    /// @details
    ///  MSDF 生成の「②エッジ彩色 → ③距離場計算 → ④矩形パッキング」を担当する。
    ///  ②③は msdfgen/core に丸投げしており、このクラスがやるのは
    ///  座標系の橋渡しとアトラスへの配置だけ。
    ///
    /// @note 生成は重い（1 グリフあたり数 ms）。ゲーム中に同期で呼ばないこと。
    ///       最小構成ではシーン初期化時に一括で焼いている。
    class MsdfFontBaker
    {
    public:
        /// @brief 指定した文字集合を焼いてアトラスを作る
        /// @param face 読み込み済みのフォント
        /// @param codePoints 焼く文字（重複していても構わない）
        /// @param settings 解像度・距離場範囲・アトラスサイズ
        static MsdfBakeResult Bake(
            const DirectWriteFontFace& face,
            const std::vector<char32_t>& codePoints,
            const MsdfBakeSettings& settings);

        /// @brief アトラスを PNG に書き出す（工程①②の目視確認用）
        /// @details ランタイムのロード経路ではない。純粋なデバッグ出力。
        ///          「グリフが正しい向き・形で焼けているか」はこれを開けば一目で分かる。
        static bool SaveAtlasPng(const MsdfBakeResult& result, const std::filesystem::path& outPath);
    };
}

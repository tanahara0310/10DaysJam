#pragma once

#include "Text/MsdfFontTypes.h"
#include "Graphics/RHI/Descriptor/DescriptorHandle.h"
#include "Math/Vector/Vector2.h"

#include <d3d12.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

namespace CoreEngine
{
    class GraphicsCore;

    /// @brief MSDF フォントの生成指定
    struct MsdfFontDesc
    {
        /// @brief フォントファイルのパス（指定するとフォールバック列の先頭になる）
        std::wstring filePath;
        uint32_t faceIndex = 0; ///< .ttc 内のフェイス番号

        /// @brief システムフォントのフォールバック列（先頭が優先）
        /// @details
        ///  「最初に見つかった 1 つを使う」ではなく **文字ごとに先頭から探す**。
        ///  先頭のフォントに無い文字は次のフォントから拾うので、
        ///  和文フォント → 欧文フォント → 記号フォント のように並べる。
        ///  行送り等のメトリクスは実際に採用できた先頭のフォントのものを使う。
        std::vector<std::wstring> systemFamilyNames;

        /// @brief アトラスへ焼く文字（UTF-8）
        /// @note 最小構成では「使う文字を全部渡す」方式。
        ///       和文を本格対応する際は、ここを動的アトラスへ置き換える。
        std::string charsetUtf8;

        /// @brief ASCII 可視文字（U+0020..U+007E）を charsetUtf8 に加えて焼く
        bool includeAscii = true;

        MsdfBakeSettings bake{};

        /// @brief 焼き上がったアトラスをディスクへ残して次回以降再利用する
        /// @note 指定・フォント・文字集合のいずれかが変われば別キーになるので、
        ///       手で消す必要は無い
        bool useDiskCache = true;

        /// @brief キャッシュの置き場所（作業ディレクトリからの相対でよい）
        std::filesystem::path cacheDirectory = "Cache/FontCache";

        /// @brief 空でなければアトラスを PNG に書き出す（目視確認用）
        std::filesystem::path debugAtlasDumpPath;
    };

    /// @brief ランタイムで使う MSDF フォントアセット
    /// @details
    ///  アトラステクスチャ（GPU）とグリフメトリクス（CPU）の対を保持する。
    ///
    /// @note アトラスは TextureManager を通さず自前で GPU へ上げている。
    ///       TextureManager の経路はミップ生成と BC3 圧縮を行うが、
    ///       どちらも距離場を破壊する（ミップ平均はコーナーの median を壊し、
    ///       ブロック圧縮は輪郭にノイズを載せる）ため通せない。
    class MsdfFont
    {
    public:
        MsdfFont() = default;
        ~MsdfFont();

        MsdfFont(const MsdfFont&) = delete;
        MsdfFont& operator=(const MsdfFont&) = delete;

        /// @brief フォントを読み込んでアトラスを生成し、GPU へ転送する
        /// @param graphicsCore デバイスとディスクリプタの供給元
        /// @param desc 生成指定
        /// @return 成功したら true
        bool Build(GraphicsCore* graphicsCore, const MsdfFontDesc& desc);

        /// @brief 使用可能か
        bool IsValid() const { return atlasHandle_.gpuHandle.ptr != 0; }

        /// @brief コードポイントからグリフ情報を引く
        /// @return アトラスに焼かれていなければ nullptr
        const MsdfGlyph* FindGlyph(char32_t codePoint) const;

        /// @brief 描画に使うグリフを解決する（未収録なら .notdef を返す）
        /// @details
        ///  未収録の文字を「描かない」で済ませると文字列が部分的に消え、
        ///  不具合として見えなくなる。必ず豆腐（□）を出して気付けるようにする。
        const MsdfGlyph& ResolveGlyph(char32_t codePoint) const;

        /// @brief 未収録文字の代わりに描くグリフ（□）
        const MsdfGlyph& GetNotdefGlyph() const { return notdefGlyph_; }

        const MsdfFontMetrics& GetMetrics() const { return metrics_; }

        /// @brief アトラス SRV の GPU ハンドル
        D3D12_GPU_DESCRIPTOR_HANDLE GetAtlasGpuHandle() const { return atlasHandle_.gpuHandle; }

        /// @brief アトラスの画素サイズ（シェーダーの screenPxRange 計算に要る）
        Vector2 GetAtlasSize() const { return atlasSize_; }

        /// @brief 距離場の有効範囲（px。シェーダーへそのまま渡す）
        float GetPxRange() const { return pxRange_; }

        /// @brief メトリクスの供給元になったフォント名（ログ・デバッグ表示用）
        const std::wstring& GetResolvedFontName() const { return resolvedFontName_; }

        /// @brief 実際に開けたフォールバック列（先頭が主フォント）
        const std::vector<std::wstring>& GetFontChainNames() const { return fontChainNames_; }

    private:
        std::unordered_map<char32_t, MsdfGlyph> glyphs_;
        MsdfGlyph notdefGlyph_{};
        MsdfFontMetrics metrics_{};

        Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture_;
        DescriptorHandle atlasHandle_{};
        Vector2 atlasSize_ = { 1.0f, 1.0f };
        float pxRange_ = 4.0f;

        std::wstring resolvedFontName_;
        std::vector<std::wstring> fontChainNames_;
        GraphicsCore* graphicsCore_ = nullptr;
    };
}

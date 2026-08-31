#pragma once

#include "Graphics/Render/UI/UIRenderer.h"
#include "Graphics/Shader/CBufferLayout.h"
#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector3.h"
#include "Math/Vector/Vector4.h"

#include <cstdint>

namespace CoreEngine
{
    /// @brief MSDF テキストの頂点 1 要素（MsdfText.VS.hlsl の入力と一致させること）
    /// @details
    ///  位置は **em 単位**（フォントサイズ 1.0 のときの大きさ）。
    ///  毎フレーム UploadRing へ積み直すので、要素は必要最小限に絞っている。
    struct TextVertex
    {
        Vector4 position;
        /// xy = アトラス UV / z = アトラス配列の何枚目か
        /// @note 枚数を頂点に載せることで、複数枚にまたがる文字列でも
        ///       ドローコールを分けずに 1 回で描ける
        Vector3 texcoord;
    };

    static constexpr Cb::Field kTextVertexFields[] = {
        CB_FIELD(TextVertex, position), CB_FIELD(TextVertex, texcoord),
    };
    CB_VERIFY_STRIDE(TextVertex, kTextVertexFields);

    /// @brief MSDF テキスト描画専用レンダラー
    /// @details
    ///  スクリーン固定の正射影・定数バッファプール・WVP 計算は UI と全く同じなので
    ///  UIRenderer を土台にし、シェーダーとサンプラーだけを差し替える。
    ///
    ///  UI と別パスに分けているのは、RenderManager が
    ///  「パス種別が変わったときだけ BeginPass（＝PSO 切り替え）」で束ねるため。
    ///  同じ UI パスに相乗りさせると、UIText の後に描かれる UIImage が
    ///  MSDF 用 PSO のまま描かれてしまう。
    class TextRenderer : public UIRenderer
    {
    public:
        /// @brief 1 つの UIText が描けるグリフ数の上限
        /// @note 共有インデックスバッファの長さがそのまま上限になる。
        ///       超えた分は切り捨てて警告を出す（黙って欠けないようにするため）
        static constexpr uint32_t kMaxGlyphsPerText = 2048;

        RenderPassType GetRenderPassType() const override { return RenderPassType::UIText; }

        /// @brief 基底の Initialize(GraphicsCore*, ResourceFactory*) を隠さない
        /// @note 下で Initialize(ID3D12Device*) を宣言すると、同名の基底オーバーロードが
        ///       名前隠蔽で見えなくなる（呼び出し側は 2 引数版を使う）
        using UIRenderer::Initialize;

        /// @brief PSO の構築に加えて、全 UIText で共有するインデックスバッファを作る
        void Initialize(ID3D12Device* device) override;

        /// @brief クワッド列用の共有インデックスバッファ
        /// @details 内容は `0,1,2 / 1,3,2` の繰り返しで全 UIText 共通。
        ///          テキストごとに持つ意味が無いのでレンダラーが 1 本だけ持つ。
        const D3D12_INDEX_BUFFER_VIEW& GetSharedIndexBufferView() const { return indexBufferView_; }

    protected:
        const wchar_t* GetVertexShaderPath() const override
        {
            return L"Engine/Assets/Shaders/UI/MsdfText.VS.hlsl";
        }

        const wchar_t* GetPixelShaderPath() const override
        {
            return L"Engine/Assets/Shaders/UI/MsdfText.PS.hlsl";
        }

        const char* GetPipelineDebugName() const override { return "MsdfText"; }

        /// @brief リニア補間 + CLAMP
        /// @details
        ///  - Linear は必須。ポイントサンプリングにすると距離場の補間が効かず、
        ///    MSDF がただの低解像度ビットマップに退化する。
        ///  - CLAMP はアトラス端でのラップ回避。パディングがあるので実害は稀だが、
        ///    グリフが端に接したときに反対側の距離場を拾うのを防ぐ。
        SamplerConfig GetSamplerConfig() const override
        {
            SamplerConfig config = SamplerConfig::Linear();
            config.addressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            config.addressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            config.addressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            return config;
        }

    private:
        /// @brief 共有インデックスバッファを生成して内容を書き込む
        void CreateSharedIndexBuffer(ID3D12Device* device);

        Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
        D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    };
}

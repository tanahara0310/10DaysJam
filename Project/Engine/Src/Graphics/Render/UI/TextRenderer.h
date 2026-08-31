#pragma once

#include "Graphics/Render/UI/UIRenderer.h"

namespace CoreEngine
{
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
        RenderPassType GetRenderPassType() const override { return RenderPassType::UIText; }

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
    };
}

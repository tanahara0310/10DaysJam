#pragma once

#include "MaterialBase.h"
#include "Graphics/Shader/CBufferLayout.h"
#include "Math/MathCore.h"

namespace CoreEngine
{
    /// @brief MSDF テキスト専用マテリアルの GPU データ
    /// @details HLSL 側 `ConstantBuffer<TextMaterial> gMaterial` とレイアウトを一致させること。
    struct TextMaterialData
    {
        Vector4 color;      ///< 塗り色（RGBA）

        /// @brief 距離場の有効範囲（px）
        /// @note アトラスを焼いたときの値をそのまま渡す。ここがずれると
        ///       アンチエイリアスの幅が合わず、輪郭が太ったり痩せたりする。
        float pxRange;

        /// @name アトラスの画素サイズ
        /// @note screenPxRange の計算に必要。UV の変化率をテクセル数へ戻すために使う。
        /// @{
        float atlasWidth;
        float atlasHeight;
        /// @}

        float padding0; ///< 16B 境界合わせ
    };

    static constexpr Cb::Field kTextMaterialDataFields[] = {
        CB_FIELD(TextMaterialData, color),
        CB_FIELD(TextMaterialData, pxRange),
        CB_FIELD(TextMaterialData, atlasWidth),
        CB_FIELD(TextMaterialData, atlasHeight),
        CB_FIELD(TextMaterialData, padding0),
    };
    CB_VERIFY_LAYOUT(TextMaterialData, kTextMaterialDataFields);

    /// @brief MSDF テキスト専用マテリアル
    class TextMaterialInstance : public MaterialBase<TextMaterialData>
    {
    public:
        /// @brief GPU 定数バッファを確保して常時 Map する
        void Initialize(ID3D12Device* device);

        void SetColor(const Vector4& color);
        Vector4 GetColor() const;

        /// @brief アトラス側のパラメータを反映する（フォント切り替え時に呼ぶ）
        void SetAtlasParams(float pxRange, const Vector2& atlasSize);
    };

} // namespace CoreEngine

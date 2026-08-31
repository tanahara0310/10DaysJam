#include "pch.h"
#include "TextMaterialInstance.h"

namespace CoreEngine
{
    void TextMaterialInstance::Initialize(ID3D12Device* device)
    {
        InitializeBuffer(device);
        materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        materialData_->pxRange = 4.0f;
        materialData_->atlasWidth = 1.0f;
        materialData_->atlasHeight = 1.0f;
        materialData_->padding0 = 0.0f;
    }

    void TextMaterialInstance::SetColor(const Vector4& color)
    {
        materialData_->color = color;
    }

    Vector4 TextMaterialInstance::GetColor() const
    {
        return materialData_->color;
    }

    void TextMaterialInstance::SetAtlasParams(float pxRange, const Vector2& atlasSize)
    {
        materialData_->pxRange = pxRange;
        materialData_->atlasWidth = atlasSize.x;
        materialData_->atlasHeight = atlasSize.y;
    }

} // namespace CoreEngine

#include "pch.h"
#include "TextRenderer.h"

#include "Graphics/RHI/Resource/ResourceFactory.h"

#include <cstdint>
#include <stdexcept>

namespace CoreEngine
{
    void TextRenderer::Initialize(ID3D12Device* device)
    {
        // PSO / ルートシグネチャは UIRenderer の実装をそのまま使う
        // （差し替えるのは GetVertexShaderPath 等のフックだけ）
        UIRenderer::Initialize(device);

        CreateSharedIndexBuffer(device);
    }

    void TextRenderer::CreateSharedIndexBuffer(ID3D12Device* device)
    {
        constexpr uint32_t kIndexCount = kMaxGlyphsPerText * 6;

        indexResource_ = ResourceFactory::CreateBufferResource(
            device, sizeof(uint32_t) * kIndexCount);
        if (!indexResource_) {
            throw std::runtime_error("Failed to create MsdfText shared index buffer");
        }

        uint32_t* indices = nullptr;
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indices));
        for (uint32_t glyph = 0; glyph < kMaxGlyphsPerText; ++glyph) {
            const uint32_t base = glyph * 4;
            uint32_t* destination = indices + glyph * 6;
            destination[0] = base + 0;
            destination[1] = base + 1;
            destination[2] = base + 2;
            destination[3] = base + 1;
            destination[4] = base + 3;
            destination[5] = base + 2;
        }
        indexResource_->Unmap(0, nullptr);

        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }
}

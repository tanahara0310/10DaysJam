#include "pch.h"
#include "UIText.h"

#include "Graphics/RHI/Resource/UploadRing.h"
#include "Graphics/Render/RenderManager.h"
#include "EngineSystem/EngineSystem.h"
#include "Text/MsdfFont.h"
#include "Text/TextEncoding.h"
#include "Utility/Logger/Logger.h"

#include <cstring>

#ifdef USE_IMGUI
#include "Editor/ImGui/Wrappers/ImGuiInput.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"
#include <imgui.h>
#include <numbers>
#endif

namespace CoreEngine
{
    using namespace CoreEngine::MathCore;

    void UIText::Initialize(MsdfFont* font, const std::string& textUtf8, const std::string& name)
    {
        if (!name.empty()) {
            SetName(name);
        }

        auto* engine = GetEngineSystem();
        auto* renderManager = engine->GetService<RenderManager>();
        renderer_ = dynamic_cast<TextRenderer*>(renderManager->GetRenderer(RenderPassType::UIText));

        font_ = font;
        textUtf8_ = textUtf8;

        if (renderer_) {
            material_ = std::make_unique<TextMaterialInstance>();
            material_->Initialize(renderer_->GetGraphicsCore()->GetDevice());
            if (font_) {
                // pxRange とアトラスサイズはシェーダーの screenPxRange 計算に要る。
                // フォントを差し替えたらここも更新すること
                material_->SetAtlasParams(font_->GetPxRange(), font_->GetAtlasSize());
            }
        }

        // 文字の左上を基準にしたほうが HUD の配置は考えやすい
        layout_.pivot = { 0.0f, 0.0f };

        RebuildGeometry();
    }

    void UIText::SetText(const std::string& textUtf8)
    {
        if (textUtf8_ == textUtf8) { return; }
        textUtf8_ = textUtf8;
        geometryDirty_ = true;
    }

    void UIText::SetLineSpacing(float scale)
    {
        if (lineSpacing_ == scale) { return; }
        lineSpacing_ = scale;
        geometryDirty_ = true;
    }

    void UIText::SetPivot(const Vector2& pivot)
    {
        if (layout_.pivot.x == pivot.x && layout_.pivot.y == pivot.y) { return; }
        layout_.pivot = pivot;
        geometryDirty_ = true;
    }

    void UIText::SetColor(const Vector4& color)
    {
        if (material_) { material_->SetColor(color); }
    }

    Vector4 UIText::GetColor() const
    {
        return material_ ? material_->GetColor() : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    void UIText::RebuildGeometry()
    {
        geometryDirty_ = false;
        vertices_.clear();
        measuredSizeEm_ = { 0.0f, 0.0f };

        if (!font_ || !font_->IsValid()) { return; }

        const std::vector<char32_t> codePoints = Utf8ToUtf32(textUtf8_);
        if (codePoints.empty()) { return; }

        // アトラスに無い文字は裏で焼いてもらう。焼き上がるとフォント側の
        // グリフ世代が進み、Draw がそれを見て再度ここへ来る
        font_->RequestGlyphs(codePoints);
        lastGlyphGeneration_ = font_->GetGlyphGeneration();

        const MsdfFontMetrics& metrics = font_->GetMetrics();
        const float lineAdvance = metrics.lineHeight * lineSpacing_;

        // ── ①行ごとの幅を測る ────────────────────────────────────
        // pivot を効かせるには文字列全体の大きさが先に要る
        std::vector<float> lineWidths;
        lineWidths.push_back(0.0f);
        size_t drawableGlyphCount = 0;

        for (char32_t codePoint : codePoints) {
            if (codePoint == U'\n') {
                lineWidths.push_back(0.0f);
                continue;
            }
            if (codePoint == U'\r') { continue; }

            // アトラスに無い文字は .notdef（□）へ倒す。
            // ここで捨てると「文字が黙って消える」ことになり、不具合に気付けない
            const MsdfGlyph glyph = font_->ResolveGlyph(codePoint);

            lineWidths.back() += glyph.advance;
            if (glyph.hasBitmap) { ++drawableGlyphCount; }
        }

        float maxWidth = 0.0f;
        for (float width : lineWidths) {
            maxWidth = (std::max)(maxWidth, width);
        }
        const float totalHeight =
            static_cast<float>(lineWidths.size() - 1) * lineAdvance
            + (metrics.ascender - metrics.descender);

        measuredSizeEm_ = { maxWidth, totalHeight };

        if (drawableGlyphCount == 0) { return; }

        // 共有インデックスバッファの長さが上限。超えた分は切り捨てる
        if (drawableGlyphCount > TextRenderer::kMaxGlyphsPerText) {
            if (!glyphLimitWarned_) {
                glyphLimitWarned_ = true;
                Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Graphics,
                    "UIText '{}': グリフ数が上限 {} を超えたため切り詰めました（要求 {}）",
                    GetName(), TextRenderer::kMaxGlyphsPerText, drawableGlyphCount);
            }
            drawableGlyphCount = TextRenderer::kMaxGlyphsPerText;
        }

        // ── ②クワッドを組む ──────────────────────────────────────
        // 全て em 単位。ここにフォントサイズは掛けない（ワールド行列側で掛ける）
        const float originX = -layout_.pivot.x * maxWidth;
        const float originY = -layout_.pivot.y * totalHeight;

        vertices_.reserve(drawableGlyphCount * 4);

        float penX = 0.0f;
        size_t lineIndex = 0;

        for (char32_t codePoint : codePoints) {
            if (codePoint == U'\n') {
                penX = 0.0f;
                ++lineIndex;
                continue;
            }
            if (codePoint == U'\r') { continue; }

            const MsdfGlyph glyph = font_->ResolveGlyph(codePoint);

            if (glyph.hasBitmap && vertices_.size() / 4 < drawableGlyphCount) {
                // UI 座標系は Y 下正。フォントの plane 境界は Y 上正なので符号を反転する
                const float baselineY =
                    originY + metrics.ascender + static_cast<float>(lineIndex) * lineAdvance;

                const float left = originX + penX + glyph.planeLeft;
                const float right = originX + penX + glyph.planeRight;
                const float top = baselineY - glyph.planeTop;
                const float bottom = baselineY - glyph.planeBottom;

                // texcoord.z にアトラス配列の枚番号を載せる。
                // 複数枚にまたがる文字列でもドローコールが分かれない
                const float page = static_cast<float>(glyph.page);

                vertices_.push_back({ { left,  bottom, 0.0f, 1.0f }, { glyph.uvLeft,  glyph.uvBottom, page } });
                vertices_.push_back({ { left,  top,    0.0f, 1.0f }, { glyph.uvLeft,  glyph.uvTop,    page } });
                vertices_.push_back({ { right, bottom, 0.0f, 1.0f }, { glyph.uvRight, glyph.uvBottom, page } });
                vertices_.push_back({ { right, top,    0.0f, 1.0f }, { glyph.uvRight, glyph.uvTop,    page } });
            }

            penX += glyph.advance;
        }
    }

    void UIText::Draw(const Camera* /*camera*/)
    {
        // RenderGraph を経由しない直接呼び出し（レガシー経路）
        DrawViewInfo view{};
        view.cmdList = renderer_ ? renderer_->GetGraphicsCore()->GetCommandList() : nullptr;
        Draw(view);
    }

    void UIText::Draw(const DrawViewInfo& view)
    {
        if (!IsActive() || !renderer_ || !material_ || !font_ || !font_->IsValid()) { return; }

        auto* commandList = view.cmdList;
        if (!commandList) { return; }

        // 実行時ベイクで新しいグリフが増えていたら組み直す。
        // これが □ から本来の字へ差し替わる瞬間
        if (font_->GetGlyphGeneration() != lastGlyphGeneration_) {
            geometryDirty_ = true;
        }
        if (geometryDirty_) { RebuildGeometry(); }
        if (vertices_.empty()) { return; }

        // 頂点はフレーム単位で巻き戻る UploadRing へ毎フレーム積み直す。
        // 自前の UPLOAD バッファを書き換える方式だと、GPU が前フレームの内容を
        // 読んでいる最中に上書きしてしまう（文字列を毎フレーム変えると必ず踏む）
        const uint32_t byteSize = static_cast<uint32_t>(sizeof(TextVertex) * vertices_.size());
        const UploadAllocation allocation =
            renderer_->GetGraphicsCore()->GetUploadRing().Allocate(byteSize, 16);
        if (!allocation.IsValid()) { return; }

        std::memcpy(allocation.cpu, vertices_.data(), byteSize);

        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
        vertexBufferView.BufferLocation = allocation.gpuAddress;
        vertexBufferView.SizeInBytes = byteSize;
        vertexBufferView.StrideInBytes = sizeof(TextVertex);

        const Vector2 screenPos = layout_.CalculateScreenPosition(renderer_->GetScreenSize());

        const size_t bufferIndex = renderer_->GetAvailableConstantBuffer();

        const Vector3 position = { screenPos.x, screenPos.y, 0.0f };
        // 頂点が em 単位なので、フォントサイズがそのままスケールになる
        const Vector3 scale = { fontSize_, fontSize_, 1.0f };
        const Vector3 rotation = { 0.0f, 0.0f, layout_.rotation };

        auto& transformData = renderer_->GetTransformDataPool()[bufferIndex];
        transformData->WVP = renderer_->CalculateWVPMatrix(position, scale, rotation);
        transformData->world = Matrix::MakeAffine(scale, rotation, position);

        commandList->SetGraphicsRootConstantBufferView(
            renderer_->GetRootParamIndex("gMaterial"),
            material_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(
            renderer_->GetRootParamIndex("TransformationMatrix"),
            renderer_->GetTransformResource(bufferIndex)->GetGPUVirtualAddress());
        commandList->SetGraphicsRootDescriptorTable(
            renderer_->GetRootParamIndex("gAtlas"),
            font_->GetAtlasGpuHandle());

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&renderer_->GetSharedIndexBufferView());

        // 文字列全体が 1 ドローコールで出る（アトラスが 1 枚なので分割が要らない）
        const UINT indexCount = static_cast<UINT>(vertices_.size() / 4 * 6);
        commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    }

#ifdef USE_IMGUI
    int UIText::GetInspectorTabs(InspectorTabDef* outTabs, int maxTabs) const
    {
        if (maxTabs < 2) { return 0; }
        outTabs[0] = { "object_data.png", "レイアウト", {0.96f,0.65f,0.14f,1.0f}, {0.96f,0.65f,0.14f,0.25f} };
        outTabs[1] = { "material.png",    "テキスト",   {0.30f,0.70f,0.90f,1.0f}, {0.30f,0.70f,0.90f,0.25f} };
        return 2;
    }

    bool UIText::DrawInspectorTabContent(int tabIndex)
    {
        bool changed = false;

        switch (tabIndex)
        {
            // ── 0: レイアウト ──────────────────────────────────────
        case 0:
        {
            UI::SectionHeader("アンカー");
            {
                static const char* kAnchorNames[] = {
                    "TopLeft",    "TopCenter",    "TopRight",
                    "MiddleLeft", "Center",       "MiddleRight",
                    "BottomLeft", "BottomCenter", "BottomRight",
                };
                int anchorIdx = static_cast<int>(layout_.anchor);
                if (ImGui::Combo("##anchor", &anchorIdx, kAnchorNames, 9)) {
                    layout_.anchor = static_cast<UIAnchor>(anchorIdx);
                    changed = true;
                }
            }

            UI::SectionHeader("AnchoredPosition");
            if (UI::DragVec2("##anchoredPos", layout_.anchoredPos, 1.0f)) {
                changed = true;
            }

            UI::SectionHeader("Pivot");
            {
                Vector2 pivotTmp = layout_.pivot;
                if (UI::DragVec2("##pivot", pivotTmp, 0.01f, 0.0f, 1.0f)) {
                    SetPivot(pivotTmp);
                    changed = true;
                }
            }

            UI::SectionHeader("回転");
            {
                float rotDeg = layout_.rotation * (180.0f / static_cast<float>(std::numbers::pi));
                if (UI::DragFloat("度##rot", rotDeg, 0.5f, -360.0f, 360.0f)) {
                    layout_.rotation = rotDeg * (static_cast<float>(std::numbers::pi) / 180.0f);
                    changed = true;
                }
            }

            UI::SectionHeader("描画順序");
            {
                int sortTmp = layout_.sortOrder;
                if (ImGui::DragInt("Sort Order##so", &sortTmp, 1, -9999, 9999)) {
                    SetSortOrder(sortTmp);
                    changed = true;
                }
            }
            break;
        }

        // ── 1: テキスト ────────────────────────────────────────
        case 1:
        {
            UI::SectionHeader("フォントサイズ");
            if (UI::DragFloat("px##fontSize", fontSize_, 0.5f, 1.0f, 512.0f)) {
                // 頂点は em 単位なので再構築不要。ここが MSDF の効きどころで、
                // 何倍にしても輪郭は鋭いまま
                changed = true;
            }

            UI::SectionHeader("行間");
            {
                float spacingTmp = lineSpacing_;
                if (UI::DragFloat("倍##lineSpacing", spacingTmp, 0.01f, 0.1f, 4.0f)) {
                    SetLineSpacing(spacingTmp);
                    changed = true;
                }
            }

            UI::SectionHeader("カラー");
            if (material_) {
                Vector4 color = material_->GetColor();
                if (UI::ColorEdit("##color", color)) {
                    material_->SetColor(color);
                    changed = true;
                }
            }

            UI::SectionHeader("情報");
            {
                const Vector2 measured = GetMeasuredSize();
                ImGui::Text("文字列: %s", textUtf8_.c_str());
                ImGui::Text("実寸: %.1f x %.1f px", measured.x, measured.y);
                ImGui::Text("グリフ数: %u / %u", GetGlyphCount(), TextRenderer::kMaxGlyphsPerText);
                ImGui::Text("頂点転送: %u B/frame",
                    static_cast<unsigned>(sizeof(TextVertex) * vertices_.size()));
                if (font_) {
                    const Vector2 atlas = font_->GetAtlasSize();
                    ImGui::Text("アトラス: %.0f x %.0f / pxRange %.1f",
                        atlas.x, atlas.y, font_->GetPxRange());
                }
            }
            break;
        }

        default: break;
        }

        return changed;
    }
#endif // USE_IMGUI
}

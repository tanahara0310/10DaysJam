#pragma once

#include "UIElement.h"
#include "GameObject/GameObject.h"
#include "Graphics/Material/TextMaterialInstance.h"
#include "Graphics/Render/UI/TextRenderer.h"
#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector4.h"

#include <memory>
#include <string>
#include <vector>

namespace CoreEngine
{
    class MsdfFont;

    /// @brief MSDF フォントで文字列を描く UI 要素
    /// @details
    ///  頂点は **em 単位**（フォントサイズ 1.0 のときの大きさ）で組み、
    ///  フォントサイズはワールド行列のスケール成分として掛ける。
    ///  そのため SetFontSize() は頂点を組み直さない。
    ///  「スケールを変えても輪郭が崩れない」は距離場が担保し、
    ///  「スケール変更が軽い」はこの構成が担保する。
    ///
    ///  頂点そのものは CPU 側に持ち、描画のたびに UploadRing（フレーム単位で
    ///  巻き戻る UPLOAD ヒープ）へ積む。自前の UPLOAD バッファを持って書き換えると、
    ///  GPU が前フレームの内容を読んでいる最中に上書きしてしまう。
    class UIText : public GameObject
    {
    public:
        UIText() = default;
        ~UIText() override = default;

        /// @brief 初期化
        /// @param font 生成済みの MSDF フォント（寿命は呼び出し側が持つ）
        /// @param textUtf8 表示文字列（UTF-8。改行 \n に対応）
        /// @param name デバッグ名（任意）
        void Initialize(MsdfFont* font, const std::string& textUtf8, const std::string& name = "");

        // ===== GameObject インターフェース =====
        RenderPassType GetRenderPassType() const override { return RenderPassType::UIText; }
        BlendMode GetBlendMode() const override { return BlendMode::kBlendModeNormal; }
        const char* GetObjectName() const override { return "UIText"; }
        void Draw(const Camera* camera) override;
        void Draw(const DrawViewInfo& view) override;

        Vector3 GetWorldPosition() const override
        {
            return { layout_.anchoredPos.x, layout_.anchoredPos.y, 0.0f };
        }

        // ===== テキスト =====

        /// @brief 表示文字列を差し替える（頂点を組み直す）
        /// @note 組み直すのは CPU 側の配列だけなので、毎フレーム呼んでも GPU 側は壊れない
        void SetText(const std::string& textUtf8);
        const std::string& GetText() const { return textUtf8_; }

        /// @brief フォントサイズ（px）を設定する
        /// @note 頂点は em 単位なので、ここを変えても再構築は起きない
        void SetFontSize(float pixelSize) { fontSize_ = pixelSize; }
        float GetFontSize() const { return fontSize_; }

        /// @brief 行間の倍率（1.0 でフォント本来の行送り）
        void SetLineSpacing(float scale);
        float GetLineSpacing() const { return lineSpacing_; }

        /// @brief 文字列を囲む矩形のサイズ（px）
        Vector2 GetMeasuredSize() const
        {
            return { measuredSizeEm_.x * fontSize_, measuredSizeEm_.y * fontSize_ };
        }

        /// @brief 描画するグリフ数
        uint32_t GetGlyphCount() const { return static_cast<uint32_t>(vertices_.size() / 4); }

        // ===== UILayout アクセサ =====
        void SetAnchor(UIAnchor anchor) { layout_.anchor = anchor; }
        UIAnchor GetAnchor() const { return layout_.anchor; }

        void SetAnchoredPosition(const Vector2& pos) { layout_.anchoredPos = pos; }
        Vector2 GetAnchoredPosition() const { return layout_.anchoredPos; }

        /// @brief 文字列全体の基準点（0,0 = 左上 / 0.5,0.5 = 中央）
        void SetPivot(const Vector2& pivot);
        Vector2 GetPivot() const { return layout_.pivot; }

        void SetUIRotation(float radians) { layout_.rotation = radians; }
        float GetUIRotation() const { return layout_.rotation; }

        void SetSortOrder(int order) { layout_.sortOrder = order; SetRenderOrder(order); }
        int  GetSortOrder() const { return layout_.sortOrder; }

        const UILayout& GetLayout() const { return layout_; }

        // ===== カラー =====
        void SetColor(const Vector4& color);
        Vector4 GetColor() const;

#ifdef USE_IMGUI
        int  GetInspectorTabs(InspectorTabDef* outTabs, int maxTabs) const override;
        bool DrawInspectorTabContent(int tabIndex) override;
#endif

    private:
        /// @brief 文字列からグリフのクワッド列（CPU 側）を組み立てる
        void RebuildGeometry();

        TextRenderer* renderer_ = nullptr;
        MsdfFont* font_ = nullptr;

        std::string textUtf8_;
        float fontSize_ = 32.0f;
        float lineSpacing_ = 1.0f;

        UILayout layout_;
        /// 文字列を囲む矩形（em 単位）。GetMeasuredSize / pivot の計算に使う
        Vector2 measuredSizeEm_ = { 0.0f, 0.0f };

        /// CPU 側の頂点。描画時に UploadRing へ積む
        std::vector<TextVertex> vertices_;

        std::unique_ptr<TextMaterialInstance> material_;

        bool geometryDirty_ = false;
        /// グリフ数上限の警告を 1 回だけ出すためのフラグ
        bool glyphLimitWarned_ = false;

        /// @brief 最後に組んだときのフォント側のグリフ世代
        /// @details 実行時ベイクでグリフ表が更新されると進む。
        ///          変化を検出したら頂点を組み直し、□ が本来の字へ差し替わる
        uint32_t lastGlyphGeneration_ = 0;
    };
}

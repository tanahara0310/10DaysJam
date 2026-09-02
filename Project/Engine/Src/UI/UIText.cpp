#include "pch.h"
#include "UIText.h"

#include "Graphics/Render/RenderManager.h"
#include "EngineSystem/EngineSystem.h"
#include "Text/FontManager.h"
#include "Text/MsdfFont.h"
#include "Text/TextEncoding.h"
#include "Text/TextLineBreak.h"
#include "Scene/SceneSaveSystem.h"
#include "Utility/JsonManager/JsonManager.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#ifdef USE_IMGUI
#include "Editor/ImGui/Wrappers/ImGuiInput.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"
#include <imgui.h>
#include <numbers>
#endif

namespace CoreEngine
{
    using namespace CoreEngine::MathCore;

    namespace
    {
        constexpr size_t kNoBreakCandidate = (std::numeric_limits<size_t>::max)();

        /// 距離場は輪郭の外側 pxRange/2 までしか情報を持たない。
        /// 端ぎりぎりは値が飽和しているので、少し内側を上限にする
        constexpr float kMaxOutlineSd = 0.45f;
    }

#ifdef USE_IMGUI
    namespace
    {
        /// @brief 入力欄の作業バッファへ文字列を写す
        /// @details 入りきらない分は捨てる。ただし UTF-8 の途中で切ると
        ///          文字化けするので、多バイト文字の境界まで戻して切る
        template <size_t N>
        void CopyToEditBuffer(std::array<char, N>& buffer, const std::string& source)
        {
            size_t length = (std::min)(source.size(), N - 1);
            while (length > 0 &&
                (static_cast<unsigned char>(source[length]) & 0xC0) == 0x80) {
                --length; // 継続バイトの上にいる間は先頭バイトまで戻す
            }
            std::memcpy(buffer.data(), source.data(), length);
            buffer[length] = '\0';
        }
    }
#endif // USE_IMGUI

    namespace
    {
        /// @brief シーン JSON から UIText を復元できるように型を登録する
        /// @note 静的初期化で 1 度だけ走る。エディタ上で追加したテキストが
        ///       次回起動時に復活するのはこの登録があるため
        const bool kUITextTypeRegistered = [] {
            SceneSaveSystem::RegisterObjectType("UIText",
                [] { return std::make_unique<UIText>(); });
            return true;
            }();
    }

    void UIText::Initialize()
    {
        auto* engine = GetEngineSystem();
        if (!engine) { return; }

        if (auto* renderManager = engine->GetService<RenderManager>()) {
            renderer_ = dynamic_cast<TextRenderer*>(
                renderManager->GetRenderer(RenderPassType::UIText));
        }

        // フォントがまだ無いなら既定フォントを取る。
        // エディタ上で追加したテキストは指定が無い状態で生まれるため
        if (!font_) {
            if (auto* fontManager = engine->GetService<FontManager>()) {
                fontName_ = FontManager::kDefaultFontName;
                font_ = fontManager->AcquireNamed(fontName_);
            }
        }

        // 文字の左上を基準にしたほうが HUD の配置は考えやすい
        layout_.pivot = { 0.0f, 0.0f };

        RebuildGeometry();
    }

    void UIText::Initialize(MsdfFont* font, const std::string& textUtf8, const std::string& name)
    {
        if (!name.empty()) {
            SetName(name);
        }

        font_ = font;
        textUtf8_ = textUtf8;

        // 共通の初期化（レンダラー解決・pivot・頂点の構築）へ合流する。
        // font_ を先に入れてあるので既定フォントの取得はスキップされる
        Initialize();
    }

    void UIText::SetFontByName(const std::string& fontName)
    {
        auto* engine = GetEngineSystem();
        if (!engine) { return; }

        auto* fontManager = engine->GetService<FontManager>();
        if (!fontManager) { return; }

        MsdfFont* resolved = fontManager->AcquireNamed(fontName);
        if (!resolved) { return; }

        fontName_ = fontName;
        font_ = resolved;
        geometryDirty_ = true;
    }

    void UIText::SetText(const std::string& textUtf8)
    {
        if (textUtf8_ == textUtf8) { return; }
        textUtf8_ = textUtf8;
        geometryDirty_ = true;
    }

    void UIText::SetFontSize(float pixelSize)
    {
        if (fontSize_ == pixelSize) { return; }
        fontSize_ = pixelSize;

        // 折り返し幅もフィールドの大きさも px 指定なので、
        // フォントサイズが変わると折り位置と枠内での揃え位置が変わる。
        // どちらも使っていなければ頂点は em 単位のまま使い回せる
        if (wrapWidthPx_ > 0.0f || !fieldAutoFit_) {
            geometryDirty_ = true;
            return;
        }
        // 自動調整なら、見た目の大きさだけ更新すれば足りる
        layout_.size = GetMeasuredSize();
    }

    void UIText::SetFieldSize(const Vector2& sizePx)
    {
        // 大きさを明示した以上、文字列に合わせて縮められては困る
        fieldAutoFit_ = false;

        if (layout_.size.x == sizePx.x && layout_.size.y == sizePx.y) { return; }
        layout_.size = sizePx;
        geometryDirty_ = true; // 幅が変われば折り位置が変わる
    }

    void UIText::SetFieldAutoFit(bool enable)
    {
        if (fieldAutoFit_ == enable) { return; }
        fieldAutoFit_ = enable;
        geometryDirty_ = true;
    }

    void UIText::SetAlign(TextAlignH horizontal, TextAlignV vertical)
    {
        if (alignH_ == horizontal && alignV_ == vertical) { return; }
        alignH_ = horizontal;
        alignV_ = vertical;
        geometryDirty_ = true;
    }

    void UIText::SetWrapWidth(float pixelWidth)
    {
        if (wrapWidthPx_ == pixelWidth) { return; }
        wrapWidthPx_ = pixelWidth;
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

    void UIText::SetOutline(const Vector4& color, float widthEm)
    {
        style_.outlineColor = color;
        style_.outlineWidthEm = (std::max)(widthEm, 0.0f);
    }

    float UIText::GetMaxOutlineWidth() const
    {
        if (!font_) { return 0.0f; }
        const float pxRange = font_->GetPxRange();
        if (pxRange <= 0.0f) { return 0.0f; }

        // em → 距離場の値。この逆数に上限値を掛けたものが表現できる最大幅
        const float sdUnitsPerEm = static_cast<float>(font_->GetGlyphPixelSize()) / pxRange;
        return (sdUnitsPerEm > 0.0f) ? (kMaxOutlineSd / sdUnitsPerEm) : 0.0f;
    }

    void UIText::BuildLines(const std::vector<char32_t>& codePoints,
        const std::vector<MsdfGlyph>& glyphs,
        float wrapWidthEm,
        std::vector<LineRange>& outLines)
    {
        outLines.clear();

        const size_t count = codePoints.size();
        const bool wrapping = wrapWidthEm > 0.0f;

        size_t lineBegin = 0;
        float lineWidth = 0.0f;
        // 「ここで折ってよい」位置と、そこまでの幅
        size_t breakCandidate = kNoBreakCandidate;
        float breakCandidateWidth = 0.0f;

        for (size_t i = 0; i < count; ++i) {
            const char32_t codePoint = codePoints[i];
            if (codePoint == U'\r') { continue; }

            if (codePoint == U'\n') {
                outLines.push_back({ lineBegin, i, lineWidth });
                lineBegin = i + 1;
                lineWidth = 0.0f;
                breakCandidate = kNoBreakCandidate;
                continue;
            }

            // この文字の直前で折れるなら候補として覚えておく。
            // 禁則（行頭に句読点・行末に開き括弧）はここで弾かれる
            if (wrapping && i > lineBegin
                && TextLineBreak::CanBreakBetween(codePoints[i - 1], codePoint)) {
                breakCandidate = i;
                breakCandidateWidth = lineWidth;
            }

            const float advance = glyphs[i].advance;

            if (wrapping && i > lineBegin && lineWidth + advance > wrapWidthEm) {
                // 候補が無ければその場で強制的に折る
                // （欧文の長い単語や、禁則で候補が潰れた場合）
                const bool hasCandidate =
                    breakCandidate != kNoBreakCandidate && breakCandidate > lineBegin;
                const size_t breakAt = hasCandidate ? breakCandidate : i;
                const float width = hasCandidate ? breakCandidateWidth : lineWidth;

                outLines.push_back({ lineBegin, breakAt, width });

                lineBegin = breakAt;
                // 行頭に残る空白は詰める
                while (lineBegin < count && codePoints[lineBegin] == U' ') {
                    ++lineBegin;
                }

                lineWidth = 0.0f;
                breakCandidate = kNoBreakCandidate;

                // 折った位置から走査し直す（lineBegin は必ず前進するので止まらない）
                i = lineBegin - 1;
                continue;
            }

            lineWidth += advance;
        }

        if (lineBegin < count || outLines.empty()) {
            outLines.push_back({ lineBegin, count, lineWidth });
        }
    }

    void UIText::RebuildGeometry()
    {
        geometryDirty_ = false;
        glyphVertices_.clear();
        measuredSizeEm_ = { 0.0f, 0.0f };
        lineCount_ = 0;

        if (!font_ || !font_->IsValid()) { return; }

        const std::vector<char32_t> codePoints = Utf8ToUtf32(textUtf8_);
        if (codePoints.empty()) { return; }

        // アトラスに無い文字は裏で焼いてもらう。焼き上がるとフォント側の
        // グリフ世代が進み、Draw がそれを見て再度ここへ来る
        font_->RequestGlyphs(codePoints);
        lastGlyphGeneration_ = font_->GetGlyphGeneration();

        // グリフ情報は先にまとめて引く。1 文字ずつ引くとフォント側のロックを
        // 文字数ぶん取ることになり、ワーカーのベイクと競合しやすい。
        // アトラスに無い文字は .notdef（□）へ倒れる。
        // ここで捨てると「文字が黙って消える」ことになり、不具合に気付けない
        std::vector<MsdfGlyph> glyphs;
        glyphs.reserve(codePoints.size());
        for (char32_t codePoint : codePoints) {
            glyphs.push_back(font_->ResolveGlyph(codePoint));
        }

        const MsdfFontMetrics& metrics = font_->GetMetrics();
        const float lineAdvance = metrics.lineHeight * lineSpacing_;

        // ── ①行に分ける（折り返し + 禁則処理）──────────────────
        // フィールドを固定しているならその幅で折る。
        // 折り返し幅を別に持たせると「枠の幅」と食い違って直感に反するため
        const float wrapWidthPx = fieldAutoFit_ ? wrapWidthPx_ : layout_.size.x;
        const float wrapWidthEm = (wrapWidthPx > 0.0f && fontSize_ > 0.0f)
            ? wrapWidthPx / fontSize_
            : 0.0f;

        std::vector<LineRange> lines;
        BuildLines(codePoints, glyphs, wrapWidthEm, lines);
        lineCount_ = static_cast<uint32_t>(lines.size());

        float maxWidth = 0.0f;
        size_t drawableGlyphCount = 0;
        for (const LineRange& line : lines) {
            maxWidth = (std::max)(maxWidth, line.width);
            for (size_t i = line.begin; i < line.end; ++i) {
                if (glyphs[i].hasBitmap) { ++drawableGlyphCount; }
            }
        }

        const float totalHeight =
            static_cast<float>(lines.size() - 1) * lineAdvance
            + (metrics.ascender - metrics.descender);

        // 折り返しているなら、囲み矩形は指定幅そのものとして扱う
        // （中央寄せ等で「指定した箱」を基準にできるようにする）
        measuredSizeEm_ = { (wrapWidthEm > 0.0f) ? wrapWidthEm : maxWidth, totalHeight };

        // 自動調整ならフィールドを文字列の大きさへ合わせる。
        // 固定しているなら layout_.size がそのままフィールドの大きさ
        if (fieldAutoFit_) {
            layout_.size = GetMeasuredSize();
        }

        // 文字を流し込む枠（em 単位）。頂点は em で組むのでここも em に揃える
        const Vector2 fieldEm = fieldAutoFit_ || fontSize_ <= 0.0f
            ? measuredSizeEm_
            : Vector2{ layout_.size.x / fontSize_, layout_.size.y / fontSize_ };

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
        // 全て em 単位。フォントサイズ・位置・回転は描画時にまとめて掛ける。
        // 原点はフィールドの左上（ピボット基準）
        const float originX = -layout_.pivot.x * fieldEm.x;

        // 縦揃え：文字列全体の高さとフィールドの高さの差を配る
        const float verticalSlack = fieldEm.y - totalHeight;
        const float alignOffsetY =
            (alignV_ == TextAlignV::Middle) ? verticalSlack * 0.5f :
            (alignV_ == TextAlignV::Bottom) ? verticalSlack : 0.0f;

        const float originY = -layout_.pivot.y * fieldEm.y + alignOffsetY;

        // 横揃え：行ごとに幅が違うので、行単位で書き出し位置をずらす
        const auto alignOffsetX = [this, &fieldEm](float lineWidth) {
            switch (alignH_) {
            case TextAlignH::Center: return (fieldEm.x - lineWidth) * 0.5f;
            case TextAlignH::Right:  return  fieldEm.x - lineWidth;
            default:                 return 0.0f;
            }
            };

        glyphVertices_.reserve(drawableGlyphCount * 4);

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const LineRange& line = lines[lineIndex];
            const float baselineY =
                originY + metrics.ascender + static_cast<float>(lineIndex) * lineAdvance;

            float penX = alignOffsetX(line.width);
            for (size_t i = line.begin; i < line.end; ++i) {
                const MsdfGlyph& glyph = glyphs[i];

                if (glyph.hasBitmap && glyphVertices_.size() / 4 < drawableGlyphCount) {
                    // UI 座標系は Y 下正。フォントの plane 境界は Y 上正なので符号を反転する
                    const float left = originX + penX + glyph.planeLeft;
                    const float right = originX + penX + glyph.planeRight;
                    const float top = baselineY - glyph.planeTop;
                    const float bottom = baselineY - glyph.planeBottom;

                    // texcoord.z にアトラス配列の枚番号を載せる。
                    // 複数枚にまたがる文字列でもドローコールが分かれない
                    const float page = static_cast<float>(glyph.page);

                    glyphVertices_.push_back({ { left,  bottom }, { glyph.uvLeft,  glyph.uvBottom, page } });
                    glyphVertices_.push_back({ { left,  top    }, { glyph.uvLeft,  glyph.uvTop,    page } });
                    glyphVertices_.push_back({ { right, bottom }, { glyph.uvRight, glyph.uvBottom, page } });
                    glyphVertices_.push_back({ { right, top    }, { glyph.uvRight, glyph.uvTop,    page } });
                }

                penX += glyph.advance;
            }
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
        if (!IsActive() || !renderer_ || !font_ || !font_->IsValid()) { return; }
        if (!view.cmdList) { return; }

        // 実行時ベイクで新しいグリフが増えていたら組み直す。
        // これが □ から本来の字へ差し替わる瞬間
        if (font_->GetGlyphGeneration() != lastGlyphGeneration_) {
            geometryDirty_ = true;
        }
        if (geometryDirty_) { RebuildGeometry(); }
        if (glyphVertices_.empty()) { return; }

        const Vector2 screenPos = layout_.CalculateScreenPosition(renderer_->GetScreenSize());

        const Vector3 position = { screenPos.x, screenPos.y, 0.0f };
        // 頂点が em 単位なので、フォントサイズがそのままスケールになる
        const Vector3 scale = { fontSize_, fontSize_, 1.0f };
        const Vector3 rotation = { 0.0f, 0.0f, layout_.rotation };
        const Matrix4x4 world = Matrix::MakeAffine(scale, rotation, position);

        // ここではドローコールを出さず、レンダラーのバッチへ積むだけ。
        // 実際の描画は EndPass（またはフォント切り替え）で 1 本にまとめて出る
        renderer_->Submit(font_, glyphVertices_.data(), glyphVertices_.size(), world, style_);
    }

    json UIText::OnSerialize() const
    {
        json j;
        j["active"] = IsActive();

        // フォントは名前だけ。実体の指定は FontManager 側に置く
        j["font"] = fontName_;
        j["text"] = textUtf8_;
        j["fontSize"] = fontSize_;
        j["lineSpacing"] = lineSpacing_;
        j["wrapWidth"] = wrapWidthPx_;

        j["fieldAutoFit"] = fieldAutoFit_;
        j["fieldSize"]["x"] = layout_.size.x;
        j["fieldSize"]["y"] = layout_.size.y;
        j["alignH"] = static_cast<int>(alignH_);
        j["alignV"] = static_cast<int>(alignV_);

        j["color"] = JsonManager::Vector4ToJson(style_.color);
        j["outlineColor"] = JsonManager::Vector4ToJson(style_.outlineColor);
        j["outlineWidth"] = style_.outlineWidthEm;
        j["weight"] = style_.weightEm;

        j["anchor"] = static_cast<int>(layout_.anchor);
        j["anchoredPos"]["x"] = layout_.anchoredPos.x;
        j["anchoredPos"]["y"] = layout_.anchoredPos.y;
        j["pivot"]["x"] = layout_.pivot.x;
        j["pivot"]["y"] = layout_.pivot.y;
        j["rotation"] = layout_.rotation;
        j["sortOrder"] = layout_.sortOrder;

        return j;
    }

    void UIText::OnDeserialize(const json& j)
    {
        if (j.contains("active")) { SetActive(j["active"].get<bool>()); }

        // フォントを先に解決する（メトリクスが決まらないとレイアウトが組めない）
        if (j.contains("font") && j["font"].is_string()) {
            SetFontByName(j["font"].get<std::string>());
        }

        textUtf8_ = JsonManager::SafeGet<std::string>(j, "text", textUtf8_);
        fontSize_ = JsonManager::SafeGet<float>(j, "fontSize", fontSize_);
        lineSpacing_ = JsonManager::SafeGet<float>(j, "lineSpacing", lineSpacing_);
        wrapWidthPx_ = JsonManager::SafeGet<float>(j, "wrapWidth", wrapWidthPx_);

        fieldAutoFit_ = JsonManager::SafeGet<bool>(j, "fieldAutoFit", fieldAutoFit_);
        if (j.contains("fieldSize")) {
            layout_.size.x = JsonManager::SafeGet<float>(j["fieldSize"], "x", layout_.size.x);
            layout_.size.y = JsonManager::SafeGet<float>(j["fieldSize"], "y", layout_.size.y);
        }
        const int alignHIndex = JsonManager::SafeGet<int>(j, "alignH", static_cast<int>(alignH_));
        if (alignHIndex >= 0 && alignHIndex <= static_cast<int>(TextAlignH::Right)) {
            alignH_ = static_cast<TextAlignH>(alignHIndex);
        }
        const int alignVIndex = JsonManager::SafeGet<int>(j, "alignV", static_cast<int>(alignV_));
        if (alignVIndex >= 0 && alignVIndex <= static_cast<int>(TextAlignV::Bottom)) {
            alignV_ = static_cast<TextAlignV>(alignVIndex);
        }

        style_.color = JsonManager::SafeGetVector4(j, "color", style_.color);
        style_.outlineColor = JsonManager::SafeGetVector4(j, "outlineColor", style_.outlineColor);
        style_.outlineWidthEm = JsonManager::SafeGet<float>(j, "outlineWidth", style_.outlineWidthEm);
        style_.weightEm = JsonManager::SafeGet<float>(j, "weight", style_.weightEm);

        const int anchorIndex = JsonManager::SafeGet<int>(j, "anchor",
            static_cast<int>(layout_.anchor));
        if (anchorIndex >= 0 && anchorIndex <= static_cast<int>(UIAnchor::BottomRight)) {
            layout_.anchor = static_cast<UIAnchor>(anchorIndex);
        }
        if (j.contains("anchoredPos")) {
            layout_.anchoredPos.x = JsonManager::SafeGet<float>(j["anchoredPos"], "x", layout_.anchoredPos.x);
            layout_.anchoredPos.y = JsonManager::SafeGet<float>(j["anchoredPos"], "y", layout_.anchoredPos.y);
        }
        if (j.contains("pivot")) {
            layout_.pivot.x = JsonManager::SafeGet<float>(j["pivot"], "x", layout_.pivot.x);
            layout_.pivot.y = JsonManager::SafeGet<float>(j["pivot"], "y", layout_.pivot.y);
        }
        layout_.rotation = JsonManager::SafeGet<float>(j, "rotation", layout_.rotation);
        SetSortOrder(JsonManager::SafeGet<int>(j, "sortOrder", layout_.sortOrder));

        geometryDirty_ = true;
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

            UI::SectionHeader("テキストフィールド");
            {
                bool autoFit = fieldAutoFit_;
                if (ImGui::Checkbox("文字に合わせる##fieldAutoFit", &autoFit)) {
                    SetFieldAutoFit(autoFit);
                    changed = true;
                }

                if (fieldAutoFit_) {
                    // 枠が文字に追従するので、折り返し幅は別に指定する
                    float wrapTmp = wrapWidthPx_;
                    if (UI::DragFloat("折り返し幅 px（0 で無効）##wrap",
                        wrapTmp, 1.0f, 0.0f, 4096.0f)) {
                        SetWrapWidth(wrapTmp);
                        changed = true;
                    }
                    ImGui::TextDisabled("枠は文字列を囲む大きさになります");
                }
                else {
                    Vector2 fieldTmp = layout_.size;
                    if (UI::DragVec2("##fieldSize", fieldTmp, 1.0f, 1.0f, 8192.0f)) {
                        SetFieldSize(fieldTmp);
                        changed = true;
                    }
                    ImGui::TextDisabled("枠の幅で折り返します");
                }
                ImGui::TextDisabled("Canvas の拡縮ギズモでも伸縮できます");

                static const char* kAlignHNames[] = { "左", "中央", "右" };
                static const char* kAlignVNames[] = { "上", "中央", "下" };
                int alignHIndex = static_cast<int>(alignH_);
                int alignVIndex = static_cast<int>(alignV_);
                bool alignChanged = ImGui::Combo("横揃え##alignH", &alignHIndex, kAlignHNames, 3);
                alignChanged |= ImGui::Combo("縦揃え##alignV", &alignVIndex, kAlignVNames, 3);
                if (alignChanged) {
                    SetAlign(static_cast<TextAlignH>(alignHIndex),
                        static_cast<TextAlignV>(alignVIndex));
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
            UI::SectionHeader("文字列");
            {
                // 入力中は ImGui がバッファを持つので、そうでない間だけ写し直す
                if (!editTextActive_) { CopyToEditBuffer(editTextBuffer_, textUtf8_); }

                const ImVec2 boxSize(-FLT_MIN, ImGui::GetTextLineHeight() * 4.5f);
                if (ImGui::InputTextMultiline("##text", editTextBuffer_.data(),
                    editTextBuffer_.size(), boxSize)) {
                    // 1 打鍵ごとに反映する。未収録の字はここで焼き足しの要求が出る
                    SetText(editTextBuffer_.data());
                    changed = true;
                }
                editTextActive_ = ImGui::IsItemActive();
                ImGui::TextDisabled("Enter で改行。日本語は IME でそのまま入力できます");
            }

            UI::SectionHeader("フォント");
            {
                auto* engine = GetEngineSystem();
                auto* fontManager = engine ? engine->GetService<FontManager>() : nullptr;

                // Engine/Assets/Font のファイルと、登録済みフォントから選ぶ
                if (fontManager) {
                    const std::vector<std::string> names = fontManager->GetSelectableFontNames();
                    if (!names.empty()) {
                        if (ImGui::BeginCombo("##fontPreset", fontName_.c_str())) {
                            for (const std::string& name : names) {
                                const bool selected = (name == fontName_);
                                if (ImGui::Selectable(name.c_str(), selected)) {
                                    SetFontByName(name);
                                    CopyToEditBuffer(editFontBuffer_, fontName_);
                                    changed = true;
                                }
                                if (selected) { ImGui::SetItemDefaultFocus(); }
                            }
                            ImGui::EndCombo();
                        }
                    }
                }

                // 任意のフォント名を直接打ち込む。
                // 未登録の名前は FontManager がシステムフォント名として解決し、
                // 見つかればその場で登録するので、保存して開き直しても同じ字体になる
                if (!editFontActive_) { CopyToEditBuffer(editFontBuffer_, fontName_); }
                if (ImGui::InputText("##fontName", editFontBuffer_.data(),
                    editFontBuffer_.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    SetFontByName(editFontBuffer_.data());
                    changed = true;
                }
                editFontActive_ = ImGui::IsItemActive();
                ImGui::TextDisabled("フォント名を入力して Enter");
                // path::string() は ANSI になるので、UTF-8 への変換は Logger を通す
                ImGui::TextDisabled("%s のファイル名か、インストール済みフォント名",
                    Logger::GetInstance().PathToUtf8(
                        FontManager::GetFontDirectory()).c_str());
            }

            UI::SectionHeader("フォントサイズ");
            {
                float sizeTmp = fontSize_;
                if (UI::DragFloat("px##fontSize", sizeTmp, 0.5f, 1.0f, 512.0f)) {
                    // 頂点は em 単位なので、折り返しが無ければ再構築不要。
                    // ここが MSDF の効きどころで、何倍にしても輪郭は鋭いまま
                    SetFontSize(sizeTmp);
                    changed = true;
                }
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
            {
                Vector4 color = style_.color;
                if (UI::ColorEdit("##color", color)) {
                    style_.color = color;
                    changed = true;
                }
            }

            UI::SectionHeader("縁取り");
            {
                Vector4 outlineColor = style_.outlineColor;
                float outlineWidth = style_.outlineWidthEm;
                const float maxWidth = GetMaxOutlineWidth();

                bool outlineChanged = UI::ColorEdit("色##outline", outlineColor);
                outlineChanged |= UI::DragFloat("太さ(em)##outline", outlineWidth,
                    0.001f, 0.0f, maxWidth);
                if (outlineChanged) {
                    SetOutline(outlineColor, outlineWidth);
                    changed = true;
                }
                ImGui::TextDisabled("上限 %.3f em（pxRange を上げると広がる）", maxWidth);
            }

            UI::SectionHeader("太さ調整");
            {
                float weight = style_.weightEm;
                if (UI::DragFloat("em##weight", weight, 0.001f, -0.05f, 0.05f)) {
                    style_.weightEm = weight;
                    changed = true;
                }
            }

            UI::SectionHeader("情報");
            {
                const Vector2 measured = GetMeasuredSize();
                ImGui::Text("実寸: %.1f x %.1f px / %u 行", measured.x, measured.y, lineCount_);
                ImGui::Text("グリフ数: %u / %u", GetGlyphCount(), TextRenderer::kMaxGlyphsPerText);
                if (renderer_) {
                    ImGui::Text("テキスト全体: %u ドローコール / %u グリフ",
                        renderer_->GetLastFrameDrawCallCount(),
                        renderer_->GetLastFrameGlyphCount());
                }
                if (font_) {
                    const Vector2 atlas = font_->GetAtlasSize();
                    ImGui::Text("アトラス: %.0f x %.0f x%d枚 / pxRange %.1f",
                        atlas.x, atlas.y, font_->GetPageCount(), font_->GetPxRange());
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

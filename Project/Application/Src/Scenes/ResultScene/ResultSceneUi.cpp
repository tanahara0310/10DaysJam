#include "pch.h"
#include "ResultSceneUi.h"

#include "Components/Result/ResultButtonAnimationComponent.h"
#include "Components/GameCore/GameResultData.h"
#include "EngineSystem/EngineSystem.h"
#include "Graphics/Render/UI/UIRenderer.h"
#include "UI/UIText.h"
#include "Utility/Tween/Tween.h"
#include "WinApp/WinApp.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ResultSceneUi
{
    using namespace CoreEngine;

    CVar<float> TitleFontSize{
        "Result.UI.TitleFontSize",
        58.0f,
        "リザルト見出しのフォントサイズ（ピクセル）",
        CVarRange{ 16.0f, 160.0f } };

    CVar<Vector2> TitlePosition{
        "Result.UI.TitlePosition",
        { 0.0f, -180.0f },
        "リザルト見出しの位置（画面中央基準・ピクセル）",
        CVarRange{ -2000.0f, 2000.0f } };

    CVar<Vector4> TitleColor{
        "Result.UI.TitleColor",
        { 1.0f, 0.92f, 0.68f, 1.0f },
        "リザルト見出しの文字色" };

    CVar<int> TitleSortOrder{
        "Result.UI.TitleSortOrder",
        1000,
        "リザルト見出しの描画順",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> ScoreFontSize{
        "Result.UI.ScoreFontSize", 52.0f,
        "スコアのフォントサイズ", CVarRange{ 16.0f, 160.0f } };

    CVar<Vector2> ScorePosition{
        "Result.UI.ScorePosition", { 0.0f, -40.0f },
        "スコアの位置", CVarRange{ -2000.0f, 2000.0f } };

    CVar<int> ScoreSortOrder{
        "Result.UI.ScoreSortOrder",
        1000,
        "スコアの描画順",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> HighScoreFontSize{
        "Result.UI.HighScoreFontSize",
        38.0f,
        "ハイスコアのフォントサイズ（ピクセル）",
        CVarRange{ 16.0f, 160.0f } };

    CVar<Vector2> HighScorePosition{
        "Result.UI.HighScorePosition",
        { 0.0f, 35.0f },
        "ハイスコアの位置",
        CVarRange{ -2000.0f, 2000.0f } };

    CVar<Vector4> HighScoreColor{
        "Result.UI.HighScoreColor",
        { 1.0f, 1.0f, 1.0f, 0.86f },
        "ハイスコアの文字色" };

    CVar<int> HighScoreSortOrder{
        "Result.UI.HighScoreSortOrder",
        1000,
        "ハイスコアの描画順",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> RecordUpdatedFontSize{
        "Result.UI.RecordUpdatedFontSize",
        42.0f,
        "記録更新のフォントサイズ（ピクセル）",
        CVarRange{ 16.0f, 160.0f } };

    CVar<Vector2> RecordUpdatedPosition{
        "Result.UI.RecordUpdatedPosition",
        { 0.0f, 105.0f },
        "記録更新の位置",
        CVarRange{ -2000.0f, 2000.0f } };

    CVar<Vector4> RecordUpdatedColor{
        "Result.UI.RecordUpdatedColor",
        { 1.0f, 0.82f, 0.25f, 1.0f },
        "記録更新の文字色" };

    CVar<int> RecordUpdatedSortOrder{
        "Result.UI.RecordUpdatedSortOrder",
        1000,
        "記録更新の描画順",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> ScoreCountUpDuration{
        "Result.UI.ScoreCountUpDuration",
        0.9f,
        "スコアのカウントアップ時間（秒）",
        CVarRange{ 0.0f, 5.0f } };

    CVar<float> RecordUpdatedFadeDuration{
        "Result.UI.RecordUpdatedFadeDuration",
        0.25f,
        "記録更新の表示にかける時間（秒）",
        CVarRange{ 0.0f, 2.0f } };

    CVar<float> MenuSlideDuration{
        "Result.UI.MenuSlideDuration", 0.55f,
        "カウントアップ後のUIのスライド時間（秒）", CVarRange{ 0.0f, 3.0f } };

    CVar<float> MenuSlideDistance{
        "Result.UI.MenuSlideDistance", 1400.0f,
        "UIが右側から入る移動距離（画面外に隠れる距離を最低限確保）",
        CVarRange{ 0.0f, 4000.0f } };

    CVar<float> ButtonFontSize{
        "Result.UI.ButtonFontSize",
        44.0f,
        "リザルトボタンのフォントサイズ（ピクセル）",
        CVarRange{ 16.0f, 128.0f } };

    CVar<Vector2> ButtonPosition{
        "Result.UI.ButtonPosition",
        { 0.0f, 230.0f },
        "リザルトボタン一覧の中央位置（画面中央基準・ピクセル）",
        CVarRange{ -2000.0f, 2000.0f } };

    CVar<float> ButtonSpacing{
        "Result.UI.ButtonSpacing",
        260.0f,
        "リザルトボタンの左右間隔（ピクセル）",
        CVarRange{ 20.0f, 600.0f } };

    CVar<Vector4> ButtonColor{
        "Result.UI.ButtonColor",
        { 1.0f, 1.0f, 1.0f, 0.88f },
        "通常時リザルトボタンの文字色" };

    CVar<int> ButtonSortOrder{
        "Result.UI.ButtonSortOrder",
        1100,
        "リザルトボタンの描画順",
        CVarRange{ 0.0f, 5000.0f } };

    Elements Build(const TextFactory& createText)
    {
        Elements elements;
        if (!createText) {
            return elements;
        }

        UIText* resultTitle = createText(
            "リザルト",
            TitleFontSize.Get(),
            UIAnchor::Center,
            TitlePosition.Get(),
            TitleColor.Get(),
            "ResultTitle");
        if (resultTitle) {
            resultTitle->SetSerializeEnabled(false);
            resultTitle->SetPivot({ 0.5f, 0.5f });
            resultTitle->SetSortOrder(TitleSortOrder.Get());
        }

        UIText* scoreText = createText(
            "スコア: 0",
            ScoreFontSize.Get(), UIAnchor::Center, ScorePosition.Get(),
            TitleColor.Get(), "ResultScore");
        if (scoreText) {
            scoreText->SetSerializeEnabled(false);
            scoreText->SetPivot({ 0.5f, 0.5f });
            scoreText->SetSortOrder(ScoreSortOrder.Get());
        }

        UIText* highScoreText = createText(
            "ハイスコア: " + std::to_string(
                GameComponents::GameResultData::GetHighScore()),
            HighScoreFontSize.Get(), UIAnchor::Center, HighScorePosition.Get(),
            HighScoreColor.Get(), "ResultHighScore");
        if (highScoreText) {
            highScoreText->SetActive(false);
            highScoreText->SetSerializeEnabled(false);
            highScoreText->SetPivot({ 0.5f, 0.5f });
            highScoreText->SetSortOrder(HighScoreSortOrder.Get());
        }

        Vector4 recordUpdatedColor = RecordUpdatedColor.Get();
        recordUpdatedColor.w = 0.0f;
        UIText* recordUpdatedText = createText(
            "記録更新！",
            RecordUpdatedFontSize.Get(), UIAnchor::Center,
            RecordUpdatedPosition.Get(), recordUpdatedColor,
            "ResultRecordUpdated");
        if (recordUpdatedText) {
            recordUpdatedText->SetActive(false);
            recordUpdatedText->SetSerializeEnabled(false);
            recordUpdatedText->SetPivot({ 0.5f, 0.5f });
            recordUpdatedText->SetSortOrder(RecordUpdatedSortOrder.Get());
        }

        elements.scoreText = scoreText;
        elements.highScoreText = highScoreText;
        elements.recordUpdatedText = recordUpdatedText;

        const Vector2 firstButtonPosition = ButtonPosition.Get();
        const float buttonSpacing = ButtonSpacing.Get();
        const auto createButton = [&createText](
            const char* label,
            const Vector2& position,
            const char* name,
            const char* tweenId) -> UIText* {
                auto* button = createText(
                    label,
                    ButtonFontSize.Get(),
                    UIAnchor::Center,
                    position,
                    ButtonColor.Get(),
                    name);
                if (!button) {
                    return nullptr;
                }

                button->SetSerializeEnabled(false);
                button->SetActive(false);
                button->SetPivot({ 0.5f, 0.5f });
                button->SetSortOrder(ButtonSortOrder.Get());
                button->AddComponent<GameComponents::ResultButtonAnimationComponent>(tweenId);
                return button;
            };

        elements.retryButton = createButton(
            "リトライ",
            { firstButtonPosition.x - buttonSpacing, firstButtonPosition.y },
            "ResultRetryButton",
            "result_retry_button");
        elements.titleButton = createButton(
            "タイトルへ",
            { firstButtonPosition.x + buttonSpacing, firstButtonPosition.y },
            "ResultTitleButton",
            "result_title_button");

        return elements;
    }

    void PlayMenuEntrance(const Elements& elements, bool showRecordUpdated,
        std::function<void()> onFinished)
    {
        const std::array<UIText*, 4> texts{
            elements.highScoreText,
            showRecordUpdated ? elements.recordUpdatedText : nullptr,
            elements.retryButton, elements.titleButton };
        std::array<float, 4> destinationX{};
        UIText* owner = nullptr;
        float distance = (std::max)(0.0f, MenuSlideDistance.Get());

        for (size_t i = 0; i < texts.size(); ++i) {
            auto* text = texts[i];
            if (!text) {
                continue;
            }
            owner = text;
            destinationX[i] = text->GetAnchoredPosition().x;
            auto* engine = text->GetEngineSystem();
            auto* renderer = engine ? engine->GetService<UIRenderer>() : nullptr;
            const float canvasWidth = renderer ? renderer->GetScreenSize().x
                : static_cast<float>(WinApp::kReferenceWidth);
            // フォントの拡大分も含めて画面外へ置く。基準解像度の変更にも追従する。
            distance = (std::max)(distance, canvasWidth * 0.5f - destinationX[i]
                + text->GetMeasuredSize().x * 0.5f + text->GetFontSize() * 2.0f);
        }

        const auto applyOffset = [texts, destinationX](const float& offset) {
            for (size_t i = 0; i < texts.size(); ++i) {
                if (auto* text = texts[i]) {
                    // Y は既存の選択ボタンの上下動に任せる。
                    text->SetAnchoredPosition({ destinationX[i] + offset,
                        text->GetAnchoredPosition().y });
                }
            }
        };

        if (!owner) {
            if (onFinished) {
                onFinished();
            }
            return;
        }

        const float duration = (std::max)(0.0f, MenuSlideDuration.Get());
        applyOffset(duration > 0.0f ? distance : 0.0f);
        for (auto* text : texts) {
            if (text) {
                text->SetActive(true);
            }
        }
        if (duration <= 0.0f) {
            if (onFinished) {
                onFinished();
            }
            return;
        }

        Tween::To<float>(distance, 0.0f, duration, applyOffset)
            .SetEase(EasingUtil::Type::EaseOutCubic)
            .SetUpdateType(TweenUpdate::Unscaled)
            .SetLink(owner)
            .SetId("result_menu_slide_in")
            .OnComplete(std::move(onFinished));
    }
}

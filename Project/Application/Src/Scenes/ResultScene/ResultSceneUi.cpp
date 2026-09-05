#include "pch.h"
#include "ResultSceneUi.h"

#include "Components/Result/ResultButtonAnimationComponent.h"
#include "UI/UIText.h"

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
        CVarRange{ 20.0f, 300.0f } };

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
            resultTitle->SetOutline({ 0.05f, 0.05f, 0.12f, 0.85f }, 0.08f);
        }

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
}

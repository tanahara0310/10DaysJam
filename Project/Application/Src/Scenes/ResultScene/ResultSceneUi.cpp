#include "pch.h"
#include "ResultSceneUi.h"

#include "Components/Result/ResultButtonAnimationComponent.h"
#include "Components/GameCore/GameResultData.h"
#include "UI/UIImage.h"
#include "UI/UIText.h"
#include "WinApp/WinApp.h"

#include <algorithm>
#include <limits>

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
        "進行距離のフォントサイズ", CVarRange{ 16.0f, 160.0f } };

    CVar<Vector2> ScorePosition{
        "Result.UI.ScorePosition", { 0.0f, -40.0f },
        "進行距離の位置", CVarRange{ -2000.0f, 2000.0f } };

    CVar<float> BackgroundPadding{
        "Result.UI.BackgroundPadding",
        56.0f,
        "リザルトUI背面の黒背景に追加する余白（ピクセル）",
        CVarRange{ 0.0f, 300.0f } };

    CVar<float> BackgroundOpacity{
        "Result.UI.BackgroundOpacity",
        0.5f,
        "リザルトUI背面の黒背景の不透明度",
        CVarRange{ 0.0f, 1.0f } };

    CVar<int> BackgroundSortOrder{
        "Result.UI.BackgroundSortOrder",
        900,
        "リザルトUI背面の黒背景の描画順",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> CinematicBarHeight{
        "Result.UI.CinematicBarHeight",
        96.0f,
        "リザルト画面上下のシネマティック黒帯の高さ（ピクセル）",
        CVarRange{ 0.0f, 400.0f } };

    CVar<int> CinematicBarSortOrder{
        "Result.UI.CinematicBarSortOrder",
        1200,
        "リザルト画面上下のシネマティック黒帯の描画順",
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

    Elements Build(const TextFactory& createText, const ImageFactory& createImage)
    {
        Elements elements;
        if (!createText && !createImage) {
            return elements;
        }

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

        const auto distanceMeters =
            GameComponents::GameResultData::GetHorizontalProgressMeters();
        UIText* scoreText = createText(
            "すすんだキョリ: " + std::to_string(distanceMeters) + " m",
            ScoreFontSize.Get(), UIAnchor::Center, ScorePosition.Get(),
            TitleColor.Get(), "ResultScore");
        if (scoreText) {
            scoreText->SetSerializeEnabled(false);
            scoreText->SetPivot({ 0.5f, 0.5f });
            scoreText->SetSortOrder(TitleSortOrder.Get());
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

        // 生成した文字の実サイズから、リザルトUI全体を囲う範囲を求める。
        // 配置用の anchoredPosition はすべて Center 基準かつ pivot は中央なので、
        // そのまま画面中央基準の矩形として扱える。
        Vector2 contentMin{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max() };
        Vector2 contentMax{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest() };
        bool hasContent = false;
        const auto includeTextBounds = [&contentMin, &contentMax, &hasContent](UIText* text) {
            if (!text) {
                return;
            }

            const Vector2 position = text->GetAnchoredPosition();
            const Vector2 size = text->GetMeasuredSize();
            contentMin.x = std::min(contentMin.x, position.x - size.x * 0.5f);
            contentMin.y = std::min(contentMin.y, position.y - size.y * 0.5f);
            contentMax.x = std::max(contentMax.x, position.x + size.x * 0.5f);
            contentMax.y = std::max(contentMax.y, position.y + size.y * 0.5f);
            hasContent = true;
        };

        includeTextBounds(resultTitle);
        includeTextBounds(scoreText);
        includeTextBounds(elements.retryButton);
        includeTextBounds(elements.titleButton);

        if (createImage && hasContent) {
            constexpr const char* kWhiteTexture =
                "Engine/Assets/Textures/Debug/white1x1.png";

            elements.background = createImage(kWhiteTexture, "ResultBackground");
            if (elements.background) {
                const float padding = BackgroundPadding.Get();
                const float backgroundWidth =
                    (contentMax.x - contentMin.x + padding * 2.0f) * 1.2f;
                elements.background->SetSerializeEnabled(false);
                elements.background->SetAnchor(UIAnchor::Center);
                elements.background->SetAnchoredPosition({
                    (contentMin.x + contentMax.x) * 0.5f,
                    (contentMin.y + contentMax.y) * 0.5f });
                elements.background->SetPivot({ 0.5f, 0.5f });
                elements.background->SetSize({
                    backgroundWidth,
                    contentMax.y - contentMin.y + padding * 2.0f });
                elements.background->SetColor({ 0.0f, 0.0f, 0.0f, BackgroundOpacity.Get() });
                elements.background->SetSortOrder(BackgroundSortOrder.Get());
            }
        }

        if (createImage) {
            constexpr const char* kWhiteTexture =
                "Engine/Assets/Textures/Debug/white1x1.png";
            constexpr float kReferenceCanvasWidth =
                static_cast<float>(CoreEngine::WinApp::kReferenceWidth);

            const auto createCinematicBar = [
                &createImage,
                kWhiteTexture,
                kReferenceCanvasWidth](
                CoreEngine::UIAnchor anchor,
                const CoreEngine::Vector2& pivot,
                const char* name) -> UIImage* {
                    auto* bar = createImage(kWhiteTexture, name);
                    if (!bar) {
                        return nullptr;
                    }

                    bar->SetSerializeEnabled(false);
                    bar->SetAnchor(anchor);
                    bar->SetAnchoredPosition({ 0.0f, 0.0f });
                    bar->SetPivot(pivot);
                    bar->SetSize({
                        kReferenceCanvasWidth,
                        CinematicBarHeight.Get() });
                    bar->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
                    bar->SetSortOrder(CinematicBarSortOrder.Get());
                    return bar;
                };

            elements.cinematicTopBar = createCinematicBar(
                UIAnchor::TopCenter,
                { 0.5f, 0.0f },
                "ResultCinematicTopBar");
            elements.cinematicBottomBar = createCinematicBar(
                UIAnchor::BottomCenter,
                { 0.5f, 1.0f },
                "ResultCinematicBottomBar");
        }

        return elements;
    }
}

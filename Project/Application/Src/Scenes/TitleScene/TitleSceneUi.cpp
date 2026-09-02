#include "pch.h"
#include "TitleSceneUi.h"

#include "Components/Title/TitleTextAnimationComponent.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"
#include "UI/UIText.h"

namespace TitleSceneUi
{
    namespace
    {
        constexpr const char* kGamepadStartPrompt = "- Aボタンをおしてスタート -";
        constexpr const char* kKeyboardStartPrompt = "- SPACEキーをおしてスタート -";
    }

    Elements Build(const TextFactory& createText, bool gamepadConnected)
    {
        Elements elements;

        if (!createText) {
            return elements;
        }

        // ボタン画像や固定の START 文字列は作らず、操作方法そのものを表示する。
        // これにより、画面を見た時点で「何を押せばよいか」が直接伝わる。
        elements.startHint = createText(
                gamepadConnected ? kGamepadStartPrompt : kKeyboardStartPrompt,
                TitleSceneCVars::HintFontSize.Get(),
                CoreEngine::UIAnchor::BottomCenter,
                TitleSceneCVars::HintPosition.Get(),
                TitleSceneCVars::HintColor.Get(),
                "StartHint");
        if (elements.startHint) {
            auto* hint = elements.startHint;
            // フォントはUIText::Initialize()が取得するFontManagerのDefaultを使う。
            // 851など特定フォントをコードで固定しないため、UITextインスペクターの
            // フォント欄から必要に応じて変更でき、保存した設定も復元できる。
            hint->SetPivot({ 0.5f, 0.5f });
            hint->SetSortOrder(TitleSceneCVars::HintSortOrder.Get());
            hint->AddComponent<GameComponents::TitleTextAnimationComponent>(
                TitleSceneCVars::HintIntroDelay.Get(),
                TitleSceneCVars::HintSlideDistance.Get(),
                TitleSceneCVars::HintIntroDuration.Get(),
                "title_start_hint_intro");
        }

        return elements;
    }

    void UpdateStartPrompt(CoreEngine::UIText* startHint, bool gamepadConnected)
    {
        if (!startHint) {
            return;
        }

        // SetText() は文字列の頂点を再構築するが、接続状態が変わった瞬間にだけ
        // 呼び出すため、通常フレームの描画負荷は増えない。
        startHint->SetText(
            gamepadConnected ? kGamepadStartPrompt : kKeyboardStartPrompt);
    }

}

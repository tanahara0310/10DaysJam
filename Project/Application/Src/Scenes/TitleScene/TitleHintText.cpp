#include "pch.h"
#include "TitleHintText.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"

namespace TitleSceneUi
{
    int TitleHintText::GetInspectorTabs(InspectorTabDef* outTabs, int maxTabs) const
    {
        // まずUIText標準の2タブを作り、その直後へヒント専用タブを追加する。
        // maxTabs が足りない場合は、UITextの標準動作を壊さないようにする。
        if (!outTabs || maxTabs <= 0) {
            return 0;
        }

        const int baseTabCount = CoreEngine::UIText::GetInspectorTabs(outTabs, maxTabs);
        if (maxTabs <= baseTabCount) {
            return baseTabCount;
        }

        outTabs[baseTabCount] = {
            "scene.png",
            "ヒント設定",
            { 0.94f, 0.56f, 0.22f, 1.0f },
            { 0.94f, 0.56f, 0.22f, 0.25f },
        };
        return baseTabCount + 1;
    }

    bool TitleHintText::DrawInspectorTabContent(int tabIndex)
    {
        constexpr int kTextInspectorTabCount = 2;
        if (tabIndex < kTextInspectorTabCount) {
            return CoreEngine::UIText::DrawInspectorTabContent(tabIndex);
        }

        if (tabIndex != kTextInspectorTabCount) {
            return false;
        }

        // Title.UI.* だけをこのUITextのインスペクターへ表示する。
        // titleオブジェクト側の設定コンポーネントでは、Transform と Animation
        // だけを表示することで、編集対象と所有オブジェクトを一致させる。
        const bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kHintCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "ここで変更した値は操作ヒントの初期値です。"
            "位置・フォント・サイズは上のUITextタブで直接編集できます。");
        return changed;
    }
}
#endif

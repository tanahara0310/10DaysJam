#include "pch.h"
#include "TitleMonkeySettingsComponent.h"

#include "Scenes/TitleScene/TitleSceneCVars.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"

namespace GameComponents
{
    bool TitleMonkeySettingsComponent::DrawInspector()
    {
        const bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kMonkeyCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "値はCVarとして保存されます。タイトルシーンを再読み込みすると"
            "monkeyの距離・登場演出へ反映されます。");
        return changed;
    }
}
#endif

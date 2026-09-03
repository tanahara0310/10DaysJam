#include "pch.h"
#include "TitleTrolleySettingsComponent.h"

#include "Scenes/TitleScene/TitleSceneCVars.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"

namespace GameComponents
{
    bool TitleTrolleySettingsComponent::DrawInspector()
    {
        const bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kTrolleyCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "値はCVarとして保存されます。タイトルシーンを再読み込みすると"
            "トロッコの配置・登場演出へ反映されます。サルの設定はmonkeyオブジェクト側にあります。");
        return changed;
    }
}
#endif

#include "pch.h"
#include "TitleCameraShakeSettingsComponent.h"

#include "Scenes/TitleScene/TitleSceneCVars.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"

namespace GameComponents
{
    bool TitleCameraShakeSettingsComponent::DrawInspector()
    {
        const bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kCameraShakeCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "値はCVarとして保存されます。タイトルシーンを再読み込みすると"
            "バウンド時のカメラシェイクへ反映されます。");
        return changed;
    }
}
#endif

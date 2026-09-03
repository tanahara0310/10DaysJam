#include "pch.h"
#include "TitleSceneSettingsComponent.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"

namespace GameComponents
{
    bool TitleSceneSettingsComponent::DrawInspector()
    {
        // titleオブジェクト自身の配置とアニメーションを表示する。
        bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kTransformCVarPrefix);
        changed |= CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kAnimationCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "値はCVarとして保存されます。タイトルシーンを再読み込みすると"
            "生成時のモデル姿勢・アニメーションへ反映されます。カメラシェイク設定は"
            "TitleCameraShakeSettingsを選択してください。");
        return changed;
    }
}
#endif

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
        // titleオブジェクトが所有するのは、モデルの配置とアニメーションだけ。
        // 操作ヒントのフォント・サイズ・位置はStartHint専用UITextのインスペクターへ
        // 分離しているため、ここではTitle.UIを描画しない。
        bool changed = CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kTransformCVarPrefix);
        changed |= CoreEngine::CVarUI::DrawTree(
            TitleSceneCVars::kAnimationCVarPrefix);

        CoreEngine::UI::Separator();
        CoreEngine::UI::Hint(
            "値はCVarとして保存されます。タイトルシーンを再読み込みすると"
            "生成時の姿勢・アニメーションへ反映されます。操作ヒントはStartHintを選択してください。");
        return changed;
    }
}
#endif

#pragma once

#include "UI/UIText.h"

namespace TitleSceneUi
{
    /// @brief タイトル画面の操作ヒント専用 UIText。
    /// @details
    ///  UIText 標準の「レイアウト」「テキスト」タブに加えて、
    ///  Title.UI CVar を表示する「ヒント設定」タブを持つ。これにより、
    ///  ヒント固有のパラメータが title.obj のインスペクターへ混ざらない。
    class TitleHintText final : public CoreEngine::UIText
    {
    public:
#ifdef USE_IMGUI
        /// @brief 標準のUITextタブへヒント専用タブを追加する。
        int GetInspectorTabs(InspectorTabDef* outTabs, int maxTabs) const override;

        /// @brief 標準タブまたはTitle.UI CVarタブの内容を描画する。
        bool DrawInspectorTabContent(int tabIndex) override;
#endif
    };
}

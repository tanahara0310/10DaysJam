#pragma once

#include "GameObject/Component/Core/IComponent.h"

namespace GameComponents
{
    /// @brief タイトルシーン用 CVar をインスペクターへ表示するコンポーネント。
    /// @details
    ///  タイトルオブジェクトへこのコンポーネントを追加すると、通常の
    ///  GameObject インスペクターに「タイトル設定」タブが現れる。値そのものは
    ///  CVarRegistry が保持するため、アニメーション実装と編集UIを分離できる。
    class TitleSceneSettingsComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleSceneSettings"; }

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "タイトル設定"; }
        const char* GetInspectorIcon() const override { return "scene.png"; }

        void GetInspectorIconColor(float* outRgba) const override
        {
            outRgba[0] = 0.94f;
            outRgba[1] = 0.56f;
            outRgba[2] = 0.22f;
            outRgba[3] = 1.0f;
        }

        /// @brief Title.* CVar の自動生成UIを描画する。
        /// @return CVar の値が変更されたら true
        bool DrawInspector() override;
#endif
    };
}

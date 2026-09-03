#pragma once

#include "GameObject/Component/Core/IComponent.h"

namespace GameComponents
{
    /// @brief monkey.obj の配置・登場演出用 CVar を monkey オブジェクトへ表示するコンポーネント。
    class TitleMonkeySettingsComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleMonkeySettings"; }

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "サル演出設定"; }
        const char* GetInspectorIcon() const override { return "scene.png"; }

        void GetInspectorIconColor(float* outRgba) const override
        {
            outRgba[0] = 0.48f;
            outRgba[1] = 0.76f;
            outRgba[2] = 0.32f;
            outRgba[3] = 1.0f;
        }

        /// @brief Title.Monkey.* CVar の自動生成UIを描画する。
        /// @return CVar の値が変更されたら true
        bool DrawInspector() override;
#endif
    };
}

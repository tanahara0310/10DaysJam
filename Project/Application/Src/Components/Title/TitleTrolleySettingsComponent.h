#pragma once

#include "GameObject/Component/Core/IComponent.h"

namespace GameComponents
{
    /// @brief トロッコ登場演出の CVar をトロッコオブジェクトへ表示するコンポーネント。
    class TitleTrolleySettingsComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleTrolleySettings"; }

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "トロッコ演出設定"; }
        const char* GetInspectorIcon() const override { return "scene.png"; }

        void GetInspectorIconColor(float* outRgba) const override
        {
            outRgba[0] = 0.94f;
            outRgba[1] = 0.56f;
            outRgba[2] = 0.22f;
            outRgba[3] = 1.0f;
        }

        /// @brief Title.Trolley.* CVar の自動生成UIを描画する。
        /// @return CVar の値が変更されたら true
        bool DrawInspector() override;
#endif
    };
}

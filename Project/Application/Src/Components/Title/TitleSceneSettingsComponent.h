#pragma once

#include "GameObject/Component/Core/IComponent.h"

namespace GameComponents
{
    /// @brief タイトルモデルの配置・アニメーション用 CVar をインスペクターへ表示するコンポーネント。
    /// @details
    ///  タイトルオブジェクトへこのコンポーネントを追加すると、通常の
    ///  GameObject インスペクターに「タイトルモデル設定」タブが現れる。
    ///  モデルに属さないカメラシェイク設定は別の空オブジェクトへ分離している。
    class TitleSceneSettingsComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleSceneSettings"; }

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "タイトルモデル設定"; }
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

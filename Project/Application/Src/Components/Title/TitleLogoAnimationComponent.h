#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    /// @brief title.obj の登場・待機アニメーションを制御するコンポーネント
    class TitleLogoAnimationComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleLogoAnimation"; }

        void Start() override;
        void OnDestroy() override;

    private:
        void OnIntroProgress(float progress);
        void PlayBounceShake(float intensity);
        void StartIdleAnimation();

        CoreEngine::TransformComponent* transform_ = nullptr;

        CoreEngine::Vector3 basePosition_{};
        CoreEngine::Vector3 baseRotation_{};
        CoreEngine::Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };

        float introDuration_ = 0.65f;
        float introScale_ = 0.82f;
        float dropHeight_ = 3.5f;
        float bobHeight_ = 0.12f;
        float bobDuration_ = 1.6f;
        float rotationAmplitude_ = 0.035f;
        float shakeStrength_ = 1.0f;

        // EaseOutBounce の接地ポイント（大きい着地 1 回 + 小さい反発 3 回）を
        // それぞれ一度だけ処理するためのインデックス。
        int nextBounceIndex_ = 0;
    };
}

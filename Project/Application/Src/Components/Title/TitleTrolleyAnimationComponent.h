#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    /// @brief trolley.obj とその上の monkey.obj を画面下から登場させるコンポーネント。
    class TitleTrolleyAnimationComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleTrolleyAnimation"; }

        void Start() override;
        void OnDestroy() override;

    private:
        void StartIdleAnimation();

        CoreEngine::TransformComponent* transform_ = nullptr;
        CoreEngine::Vector3 basePosition_{};
        float introDuration_ = 0.9f;
        float introOffset_ = 8.0f;
        CoreEngine::Vector3 bobStart_{};
        CoreEngine::Vector3 bobEnd_{ 0.0f, 0.12f, 0.0f };
        float bobDuration_ = 1.6f;
    };
}

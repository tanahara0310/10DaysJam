#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector2.h"

#include <limits>

namespace CoreEngine
{
    class UIText;
}

namespace GameComponents
{
    class HungerComponent;
    class TrainMovementComponent;

    /// @brief スタミナ・進行ブロック数・サル数をUITextへ反映するHUDコンポーネント。
    class HungerUIComponent final : public CoreEngine::IComponent
    {
    public:
        explicit HungerUIComponent(
            HungerComponent* hunger = nullptr,
            TrainMovementComponent* train = nullptr)
            : hunger_(hunger), train_(train) {}

        const char* GetTypeName() const override { return "HungerUI"; }

        void Start() override;
        void Update() override;
        void PlayInsufficientShake();

    private:
        void RefreshText();

        HungerComponent* hunger_ = nullptr;
        TrainMovementComponent* train_ = nullptr;
        CoreEngine::UIText* text_ = nullptr;
        int displayedHunger_ = (std::numeric_limits<int>::min)();
        uint32_t displayedProgress_ = (std::numeric_limits<uint32_t>::max)();
        std::size_t displayedMonkeyCount_ = (std::numeric_limits<std::size_t>::max)();
        CoreEngine::Vector2 basePosition_{};
        float shakeRemaining_ = 0.0f;
        float shakeDuration_ = 0.35f;
        float shakeAmplitude_ = 8.0f;
        float shakeFrequency_ = 55.0f;
    };
}

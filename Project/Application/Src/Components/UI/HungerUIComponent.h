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

    /// @brief 現在の空腹値をUITextへ反映するHUDコンポーネント。
    class HungerUIComponent final : public CoreEngine::IComponent
    {
    public:
        explicit HungerUIComponent(HungerComponent* hunger = nullptr)
            : hunger_(hunger) {}

        const char* GetTypeName() const override { return "HungerUI"; }

        void Start() override;
        void Update() override;
        void PlayInsufficientShake();

    private:
        void RefreshText();

        HungerComponent* hunger_ = nullptr;
        CoreEngine::UIText* text_ = nullptr;
        int displayedHunger_ = (std::numeric_limits<int>::min)();
        CoreEngine::Vector2 basePosition_{};
        float shakeRemaining_ = 0.0f;
        float shakeDuration_ = 0.35f;
        float shakeAmplitude_ = 8.0f;
        float shakeFrequency_ = 55.0f;
    };
}

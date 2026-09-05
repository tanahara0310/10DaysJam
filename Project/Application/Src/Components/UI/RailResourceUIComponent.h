#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector2.h"

#include <cstdint>
#include <limits>

namespace CoreEngine
{
    class UIText;
}

namespace GameComponents
{
    class RailResourceManagerComponent;

    // 残りレール数をUITextへ反映するHUDコンポーネント
    class RailResourceUIComponent final : public CoreEngine::IComponent {
    public:
        explicit RailResourceUIComponent(
            RailResourceManagerComponent* resourceManager = nullptr)
            : resourceManager_(resourceManager) {
        }

        const char* GetTypeName() const override {
            return "RailResourceUI";
        }

        void Start() override;
        void Update() override;
        void PlayInsufficientShake();

    private:
        void RefreshText();

        RailResourceManagerComponent* resourceManager_ = nullptr;
        CoreEngine::UIText* text_ = nullptr;
        uint32_t displayedResourceCount_ =
            (std::numeric_limits<uint32_t>::max)();
        CoreEngine::Vector2 basePosition_{};
        float shakeRemaining_ = 0.0f;
        float shakeDuration_ = 0.35f;
        float shakeAmplitude_ = 8.0f;
        float shakeFrequency_ = 55.0f;
    };
}

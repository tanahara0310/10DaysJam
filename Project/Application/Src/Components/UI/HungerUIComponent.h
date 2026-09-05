#pragma once

#include "GameObject/Component/Core/IComponent.h"

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

    private:
        void RefreshText();

        HungerComponent* hunger_ = nullptr;
        CoreEngine::UIText* text_ = nullptr;
        int displayedHunger_ = (std::numeric_limits<int>::min)();
    };
}

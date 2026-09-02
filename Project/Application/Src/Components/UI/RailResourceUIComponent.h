#pragma once

#include "GameObject/Component/Core/IComponent.h"

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

    private:
        void RefreshText();

        RailResourceManagerComponent* resourceManager_ = nullptr;
        CoreEngine::UIText* text_ = nullptr;
        uint32_t displayedResourceCount_ =
            (std::numeric_limits<uint32_t>::max)();
    };
}

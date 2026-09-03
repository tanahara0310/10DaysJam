#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    /// @brief trolley.obj の到着後、monkey.obj を OutBack でトロッコ上へ出すコンポーネント。
    class TitleMonkeyAnimationComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "TitleMonkeyAnimation"; }

        void Start() override;
        void OnDestroy() override;

    private:
        CoreEngine::TransformComponent* transform_ = nullptr;
        CoreEngine::Vector3 basePosition_{};
        float introDuration_ = 0.5f;
        float introOffset_ = 1.5f;
    };
}

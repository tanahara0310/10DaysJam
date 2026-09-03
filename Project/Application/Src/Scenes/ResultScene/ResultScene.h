#pragma once

#include "Camera/CameraManager.h"
#include "Scene/BaseScene.h"

#include <memory>
#include <vector>

namespace CoreEngine {
}

namespace ResultScene
{
    class ResultScene : public CoreEngine::BaseScene {
    public:
        ResultScene() = default;
        ~ResultScene() override;

        void OnInitialize() override;

        void OnUpdate() override;

    private:
    };
}

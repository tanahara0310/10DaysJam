#pragma once

#include "Scene/BaseScene.h"

#include <memory>
#include <vector>

namespace CoreEngine {
}

namespace GameScene
{
    class GameScene : public CoreEngine::BaseScene {
    public:
        GameScene() = default;
        ~GameScene() override;

        void OnInitialize() override;

        void OnUpdate() override;

    private:
    };
}

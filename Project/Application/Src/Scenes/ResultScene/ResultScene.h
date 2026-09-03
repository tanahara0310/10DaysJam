#pragma once

#include "Camera/CameraManager.h"
#include "Scene/BaseScene.h"

#include <memory>
#include <vector>

namespace CoreEngine {
    class UIText;
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
        enum class Selection { Retry, Title };
        void RefreshSelection();

        Selection selection_ = Selection::Retry;
        CoreEngine::UIText* retryText_ = nullptr;
        CoreEngine::UIText* titleText_ = nullptr;
        bool returnRequested_ = false;
    };
}

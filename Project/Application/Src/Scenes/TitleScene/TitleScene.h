#pragma once

#include "Scene/BaseScene.h"

namespace CoreEngine
{
    class UIText;
}

namespace TitleScene
{
    /// @brief ゲーム起動時に表示するタイトルシーン
    class TitleScene final : public CoreEngine::BaseScene {
    public:
        void OnInitialize() override;
        void OnUpdate() override;

    private:
        void StartGame();

        CoreEngine::UIText* startHint_ = nullptr;
        bool gamepadConnected_ = false;
        bool startRequested_ = false;
    };
}

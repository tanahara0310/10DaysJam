#pragma once

#include "Audio/SoundInstance.h"
#include "Scene/BaseScene.h"

namespace CoreEngine
{
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

        void SetSelection(Selection selection, bool playReaction);
        void ConfirmSelection();

        Selection selection_ = Selection::Retry;
        bool returnRequested_ = false;
        CoreEngine::UIText* retryButton_ = nullptr;
        CoreEngine::UIText* titleButton_ = nullptr;
        CoreEngine::ScopedSound resultBgm_;
    };
}

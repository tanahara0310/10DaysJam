#pragma once

#include "Audio/SoundInstance.h"
#include "Scene/BaseScene.h"

#include <cstdint>

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

        void OnFinalize() override;

    private:
        enum class Selection { Retry, Title };

        void SetSelection(Selection selection, bool playReaction);
        void ConfirmSelection();
        void StartScorePresentation();
        void CompleteScorePresentation();
        void ShowRecordUpdated();

        Selection selection_ = Selection::Retry;
        bool returnRequested_ = false;
        bool menuEntranceStarted_ = false;
        bool menuReady_ = false;
        uint32_t resultScore_ = 0;
        bool isNewHighScore_ = false;
        CoreEngine::UIText* scoreText_ = nullptr;
        CoreEngine::UIText* highScoreText_ = nullptr;
        CoreEngine::UIText* recordUpdatedText_ = nullptr;
        CoreEngine::UIText* retryButton_ = nullptr;
        CoreEngine::UIText* titleButton_ = nullptr;
        CoreEngine::ScopedSound resultBgm_;
    };
}

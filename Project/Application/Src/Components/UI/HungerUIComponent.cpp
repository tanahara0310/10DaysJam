#include "pch.h"
#include "HungerUIComponent.h"

#include "Components/GameCore/HungerComponent.h"
#include "GameObject/GameObject.h"
#include "UI/UIText.h"
#include "Utility/Logger/Logger.h"
#include "Utility/FrameRate/Time.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace CoreEngine;

void GameComponents::HungerUIComponent::Start()
{
    text_ = dynamic_cast<UIText*>(GetOwner());
    if (!text_ || !hunger_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "HungerUIComponent: UIText または Hunger が未設定です");
        SetEnabled(false);
        return;
    }
    basePosition_ = text_->GetAnchoredPosition();
    RefreshText();
}

void GameComponents::HungerUIComponent::Update()
{
    if (!text_ || !hunger_) {
        return;
    }

    const int currentHunger = static_cast<int>(std::ceil(hunger_->GetCurrentHunger()));
    if (displayedHunger_ != currentHunger) {
        RefreshText();
    }

    if (shakeRemaining_ > 0.0f) {
        shakeRemaining_ = std::max(0.0f, shakeRemaining_ - Time::UnscaledDeltaTime());
        const float elapsed = shakeDuration_ - shakeRemaining_;
        const float damping = shakeDuration_ > 0.0f ? shakeRemaining_ / shakeDuration_ : 0.0f;
        text_->SetAnchoredPosition({
            basePosition_.x + std::sin(elapsed * shakeFrequency_) * shakeAmplitude_ * damping,
            basePosition_.y
        });
        if (shakeRemaining_ <= 0.0f) {
            text_->SetAnchoredPosition(basePosition_);
        }
    }
}

void GameComponents::HungerUIComponent::PlayInsufficientShake() {
    shakeRemaining_ = shakeDuration_;
}

void GameComponents::HungerUIComponent::RefreshText()
{
    displayedHunger_ = static_cast<int>(std::ceil(hunger_->GetCurrentHunger()));
    text_->SetText("空腹値: " + std::to_string(displayedHunger_));
}

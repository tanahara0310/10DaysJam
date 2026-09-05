#include "pch.h"
#include "RailResourceUIComponent.h"

#include "GameObject/GameObject.h"
#include "UI/UIText.h"
#include "Components/Rail/RailResourceManagerComponent.h"
#include "Utility/Logger/Logger.h"
#include "Utility/FrameRate/Time.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace CoreEngine;

void GameComponents::RailResourceUIComponent::Start() {
    text_ = dynamic_cast<UIText*>(GetOwner());
    if (!text_ || !resourceManager_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "RailResourceUIComponent: UIText または RailResourceManager が未設定です");
        SetEnabled(false);
        return;
    }

    basePosition_ = text_->GetAnchoredPosition();
    RefreshText();
}

void GameComponents::RailResourceUIComponent::Update() {
    if (!text_ || !resourceManager_) {
        return;
    }

    if (displayedResourceCount_ != resourceManager_->GetResourceCount()) {
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

void GameComponents::RailResourceUIComponent::PlayInsufficientShake() {
    shakeRemaining_ = shakeDuration_;
}

void GameComponents::RailResourceUIComponent::RefreshText() {
    displayedResourceCount_ = resourceManager_->GetResourceCount();
    text_->SetText(
        "残りレール: " + std::to_string(displayedResourceCount_));
}

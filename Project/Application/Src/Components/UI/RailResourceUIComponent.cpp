#include "pch.h"
#include "RailResourceUIComponent.h"

#include "GameObject/GameObject.h"
#include "UI/UIText.h"
#include "Components/Rail/RailResourceManagerComponent.h"
#include "Utility/Logger/Logger.h"

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

    RefreshText();
}

void GameComponents::RailResourceUIComponent::Update() {
    if (!text_ || !resourceManager_) {
        return;
    }

    if (displayedResourceCount_ != resourceManager_->GetResourceCount()) {
        RefreshText();
    }
}

void GameComponents::RailResourceUIComponent::RefreshText() {
    displayedResourceCount_ = resourceManager_->GetResourceCount();
    text_->SetText(
        "残りレール: " + std::to_string(displayedResourceCount_));
}

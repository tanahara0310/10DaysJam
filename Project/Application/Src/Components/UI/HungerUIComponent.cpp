#include "pch.h"
#include "HungerUIComponent.h"

#include "Components/GameCore/HungerComponent.h"
#include "GameObject/GameObject.h"
#include "UI/UIText.h"
#include "Utility/Logger/Logger.h"

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
}

void GameComponents::HungerUIComponent::RefreshText()
{
    displayedHunger_ = static_cast<int>(std::ceil(hunger_->GetCurrentHunger()));
    text_->SetText("空腹値: " + std::to_string(displayedHunger_));
}

#include "pch.h"
#include "RailResourceManagerComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>

using namespace CoreEngine;

void GameComponents::RailResourceManagerComponent::Start() {
    // 初期リソース数をログに出力
        Logger::GetInstance().Infof(
        LogCategory::Game,
        "RailResourceManagerComponent: Initial resource count = {}",
        resourceCount_);
}

void GameComponents::RailResourceManagerComponent::Update() {
}

void GameComponents::RailResourceManagerComponent::AddResource(uint32_t amount) {
    resourceCount_ += amount;
        Logger::GetInstance().Infof(
        LogCategory::Game,
        "RailResourceManagerComponent: Added {} resources, new count = {}",
        amount, resourceCount_);
}

bool GameComponents::RailResourceManagerComponent::UseResource(uint32_t amount) {
    if (resourceCount_ >= amount) {
        resourceCount_ -= amount;
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailResourceManagerComponent: Used {} resources, new count = {}",
            amount, resourceCount_);
        return true;
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailResourceManagerComponent: Not enough resources to use {}, current count = {}",
            amount, resourceCount_);
        return false;
    }
}

bool GameComponents::RailResourceManagerComponent::HasEnoughResource(uint32_t amount) const {
    return resourceCount_ >= amount;
}

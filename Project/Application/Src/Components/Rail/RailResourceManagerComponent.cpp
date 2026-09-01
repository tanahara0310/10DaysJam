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
        "RailResourceManagerComponent: Initial resource count = %u",
        resourceCount_);
}

void GameComponents::RailResourceManagerComponent::Update() {
    // リソース数が0以下にならないようにする
    if (resourceCount_ < 0) {
        resourceCount_ = 0;
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailResourceManagerComponent: Resource count went below zero, resetting to 0");
    }
}

void GameComponents::RailResourceManagerComponent::AddResource(uint32_t amount) {
    resourceCount_ += amount;
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "RailResourceManagerComponent: Added %u resources, new count = %u",
        amount, resourceCount_);
}

void GameComponents::RailResourceManagerComponent::UseResource(uint32_t amount) {
    if (resourceCount_ >= amount) {
        resourceCount_ -= amount;
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailResourceManagerComponent: Used %u resources, new count = %u",
            amount, resourceCount_);
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailResourceManagerComponent: Not enough resources to use %u, current count = %u",
            amount, resourceCount_);
    }
}

bool GameComponents::RailResourceManagerComponent::HasEnoughResource(uint32_t amount) const {
    return resourceCount_ >= amount;
}

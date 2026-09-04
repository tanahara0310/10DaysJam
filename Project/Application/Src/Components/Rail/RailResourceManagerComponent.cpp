#include "pch.h"
#include "RailResourceManagerComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::RailResourceManagerComponent::OnSerialize() const {
    return { { "initialResourceCount", initialResourceCount_ } };
}

void GameComponents::RailResourceManagerComponent::OnDeserialize(const json& j) {
    initialResourceCount_ = JsonManager::SafeGet<uint32_t>(
        j, "initialResourceCount", initialResourceCount_);
    resourceCount_ = initialResourceCount_;
}

#ifdef USE_IMGUI
bool GameComponents::RailResourceManagerComponent::DrawInspector() {
    int initialCount = static_cast<int>(initialResourceCount_);
    bool changed = false;
    if (ImGui::DragInt("初期レール数", &initialCount, 1.0f, 0, 9999)) {
        initialResourceCount_ = static_cast<uint32_t>(std::max(initialCount, 0));
        resourceCount_ = initialResourceCount_;
        changed = true;
    }
    ImGui::TextDisabled("現在のレール数: %u", resourceCount_);
    return changed;
}
#endif

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

#include "pch.h"
#include "RailPathComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

using namespace CoreEngine;

GameComponents::RailPathComponent::RailPathComponent(uint32_t mapSizeZ, uint32_t startX, uint32_t startZ) :
    mapSizeZ_(mapSizeZ) {
    // 初期位置もビルダーが通ったマスとしてレールへ登録する
    if (startZ < mapSizeZ_) {
        railMap_.emplace_back(startX, startZ);
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailPathComponent: initial position out of bounds ({}, {})",
            startX, startZ);
    }
}

void GameComponents::RailPathComponent::Start() {
}

void GameComponents::RailPathComponent::Update() {
}

bool GameComponents::RailPathComponent::PlaceRail(
    int32_t x, int32_t z, uint32_t resourceCost) {
    if (x >= 0 && z >= 0 && z < static_cast<int32_t>(mapSizeZ_)) {
        railUndoStack_.emplace_back(x, z);
        railUndoCosts_.push_back(resourceCost);
        trainRouteQueue_.emplace_back(x, z);
        return true;
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "PlaceRail: out of bounds");
        return false;
    }
}

GameComponents::RailUndoResult GameComponents::RailPathComponent::UndoLastRailPlacement() {
    if (railUndoStack_.empty() || railUndoCosts_.empty()) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "UndoLastRailPlacement: no rail to undo");
        return {};
    }

    RailUndoResult result;
    result.succeeded = true;
    result.removedPosition = railUndoStack_.back();
    result.refundAmount = railUndoCosts_.back();

    railUndoStack_.pop_back();
    railUndoCosts_.pop_back();

    if (!trainRouteQueue_.empty() &&
        trainRouteQueue_.back() == result.removedPosition) {
        trainRouteQueue_.pop_back();
    }

    result.builderPosition = !railUndoStack_.empty()
        ? railUndoStack_.back()
        : railMap_.back();
    return result;
}

bool GameComponents::RailPathComponent::ConfirmNextRailPlacement() {
    if (trainRouteQueue_.empty()) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "ConfirmNextRailPlacement: no rail to confirm");
        return false;
    }

    const auto position = trainRouteQueue_.front();

    // 駅によって既に確定済みなら、列車用キューだけを進める。
    const bool alreadyConfirmed =
        std::find(railMap_.begin(), railMap_.end(), position) != railMap_.end();
    if (!alreadyConfirmed) {
        if (railUndoStack_.empty() || railUndoStack_.front() != position) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "ConfirmNextRailPlacement: rail queues are inconsistent");
            return false;
        }
        railMap_.push_back(position);
        railUndoStack_.erase(railUndoStack_.begin());
        railUndoCosts_.erase(railUndoCosts_.begin());
    }

    trainRouteQueue_.erase(trainRouteQueue_.begin());
    return true;
}

void GameComponents::RailPathComponent::ConfirmAllPendingRailPlacements() {
    railMap_.insert(
        railMap_.end(), railUndoStack_.begin(), railUndoStack_.end());
    railUndoStack_.clear();
    railUndoCosts_.clear();
}

bool GameComponents::RailPathComponent::TryGetNextUnconfirmedRail(
    std::pair<int32_t, int32_t>& destination) const {
    if (trainRouteQueue_.empty()) {
        return false;
    }

    destination = trainRouteQueue_.front();
    return true;
}

std::size_t GameComponents::RailPathComponent::GetUnconfirmedRailCount() const {
    return trainRouteQueue_.size();
}

std::vector<std::pair<int32_t, int32_t>>& GameComponents::RailPathComponent::GetRailMap() {
    return railMap_;
}

uint32_t GameComponents::RailPathComponent::GetMapSizeZ() const {
    return mapSizeZ_;
}

const std::vector<std::pair<int32_t, int32_t>>&
GameComponents::RailPathComponent::GetRailUndoStack() const {
    return railUndoStack_;
}

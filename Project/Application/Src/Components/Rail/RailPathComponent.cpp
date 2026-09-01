#include "pch.h"
#include "RailPathComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

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

void GameComponents::RailPathComponent::PlaceRail(int32_t x, int32_t z) {
    if (x >= 0 && z >= 0 && z < static_cast<int32_t>(mapSizeZ_)) {
        // レールを設置する際に、Undoスタックに追加する
        railUndoStack_.emplace_back(x, z);
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "PlaceRail: out of bounds");
    }
}

void GameComponents::RailPathComponent::RemoveRail(int32_t x, int32_t z) {
    if (x >= 0 && z >= 0 && z < static_cast<int32_t>(mapSizeZ_)) {
        // Undoスタックから該当するレールの座標を削除する
        railUndoStack_.erase(std::remove_if(railUndoStack_.begin(), railUndoStack_.end(), [x, z](const std::pair<int32_t, int32_t>& rail) {
            return rail.first == x && rail.second == z;
        }), railUndoStack_.end());
    } else {
        // out of bounds の場合は警告ログを出力する
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RemoveRail: out of bounds");
    }
}

std::pair<int32_t, int32_t> GameComponents::RailPathComponent::UndoLastRailPlacement() {
    if (railUndoStack_.size() > 1) {
        auto [x, z] = railUndoStack_.back();
        railUndoStack_.pop_back();
        RemoveRail(x, z);

        // Undo後の最新のレールの座標を返す
        if (railUndoStack_.empty()) {
            return { -1, -1 };
        } else {
            return railUndoStack_.back();
        }

    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "UndoLastRailPlacement: no rail to undo");
        return { -1, -1 };
    }
}

bool GameComponents::RailPathComponent::ConfirmNextRailPlacement() {
    if (!railUndoStack_.empty()) {
        auto [x, z] = railUndoStack_.front();
        railUndoStack_.erase(railUndoStack_.begin());
        railMap_.emplace_back(x, z);
        return true;
    } else {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "ConfirmNextRailPlacement: no rail to confirm");
        return false;
    }
}

bool GameComponents::RailPathComponent::TryGetNextUnconfirmedRail(
    std::pair<int32_t, int32_t>& destination) const {
    if (railUndoStack_.empty()) {
        return false;
    }

    destination = railUndoStack_.front();
    return true;
}

std::size_t GameComponents::RailPathComponent::GetUnconfirmedRailCount() const {
    return railUndoStack_.size();
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

#include "pch.h"
#include "RailViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/Utility/ModelRenderPoolComponent.h"
#include "Components/Camera/CameraManagerComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "Graphics/Line/LineManager.h"


using namespace CoreEngine;

void GameComponents::RailViewComponent::Start() {
    transform_ = Sibling<TransformComponent>();
}

void GameComponents::RailViewComponent::Update() {
    // TransformComponent がアタッチされていない場合は処理を中断する
    if (!transform_) {
        return;
    }
    // RailPathComponent がアタッチされていない場合は処理を中断する
    if (!railPath_) {
        return;
    }

    DrawRailModels();
    
    // LineManager のインスタンスを取得する
    auto& lines = LineManager::GetInstance();

    // 確定しているレールの座標を取得する
    const auto& railMap = railPath_->GetRailMap();
    for (size_t i = 0; i + 1 < railMap.size(); ++i) {
        float x = static_cast<float>(railMap[i].first) * gridSize_;
        float z = static_cast<float>(railMap[i].second) * gridSize_;

        float nextX = static_cast<float>(railMap[i + 1].first) * gridSize_;
        float nextZ = static_cast<float>(railMap[i + 1].second) * gridSize_;

        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 0.0f, 0.0f }, 1.0f, true);
    }

    // 確定していないレールの座標を取得する
    const auto& railUndoStack = railPath_->GetRailUndoStack();
    for (size_t i = 0; i + 1 < railUndoStack.size(); ++i) {
        float x = static_cast<float>(railUndoStack[i].first) * gridSize_;
        float z = static_cast<float>(railUndoStack[i].second) * gridSize_;

        float nextX = static_cast<float>(railUndoStack[i + 1].first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack[i + 1].second) * gridSize_;

        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 1.0f, 1.0f }, 1.0f, true);
    }

    // 確定したレールと確定していないレールの間のラインを描画する
    if (!railMap.empty() && !railUndoStack.empty()) {
        float x = static_cast<float>(railMap.back().first) * gridSize_;
        float z = static_cast<float>(railMap.back().second) * gridSize_;
        float nextX = static_cast<float>(railUndoStack.front().first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack.front().second) * gridSize_;
        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 1.0f, 0.0f }, 1.0f, true);
    }
}

void GameComponents::RailViewComponent::DrawRailModels() {
    if (!railPool_ || !railLeftPool_ || !railRightPool_) {
        return;
    }

    using GridPosition = std::pair<int32_t, int32_t>;

    // 確定・未確定を連結し、1本の経路としてモデルの形状を判定する。
    const auto& confirmedRails = railPath_->GetRailMap();
    const auto& pendingRails = railPath_->GetRailUndoStack();
    std::vector<GridPosition> railPath;
    railPath.reserve(confirmedRails.size() + pendingRails.size());
    railPath.insert(railPath.end(), confirmedRails.begin(), confirmedRails.end());
    railPath.insert(railPath.end(), pendingRails.begin(), pendingRails.end());

    if (railPath.empty()) {
        return;
    }

    int32_t minVisibleX = 0;
    int32_t maxVisibleX = (std::numeric_limits<int32_t>::max)();
    if (cameraManager_ && gridSize_ > 0.0f) {
        const float centerGridX =
            cameraManager_->GetFocusPosition().x / gridSize_;
        minVisibleX = std::max(
            0,
            static_cast<int32_t>(std::floor(centerGridX)) -
                static_cast<int32_t>(viewDistanceX_));
        maxVisibleX =
            static_cast<int32_t>(std::ceil(centerGridX)) +
            static_cast<int32_t>(viewDistanceX_);
    }

    const auto directionBetween = [](const GridPosition& from, const GridPosition& to) {
        return GridPosition{ to.first - from.first, to.second - from.second };
    };
    const auto yawFromDirection = [](const GridPosition& direction) {
        // モデルの前方が+Zなので、+Zを0ラジアンとしてY軸回転を求める。
        return std::atan2(
            static_cast<float>(direction.first),
            static_cast<float>(direction.second));
    };

    for (std::size_t i = 0; i < railPath.size(); ++i) {
        const GridPosition current = railPath[i];
        if (current.first < minVisibleX || current.first > maxVisibleX) {
            continue;
        }

        const bool hasPrevious = i > 0;
        const bool hasNext = i + 1 < railPath.size();
        GridPosition incoming = { 0, 1 };
        GridPosition outgoing = { 0, 1 };

        if (hasPrevious) {
            incoming = directionBetween(railPath[i - 1], current);
        }
        if (hasNext) {
            outgoing = directionBetween(current, railPath[i + 1]);
        }
        if (!hasPrevious && hasNext) {
            incoming = outgoing;
        } else if (hasPrevious && !hasNext) {
            outgoing = incoming;
        }

        const Vector3 position = {
            static_cast<float>(current.first) * gridSize_,
            0.6f,
            static_cast<float>(current.second) * gridSize_
        };

        float scaleOffset = 0.7f;
        const Vector3 scale = { scaleOffset, scaleOffset, scaleOffset };

        // XZ平面の外積。正なら進行方向に対して左折、負なら右折。
        const int32_t turn =
            incoming.first * outgoing.second -
            incoming.second * outgoing.first;
        if (hasPrevious && hasNext && turn > 0) {
            railLeftPool_->Draw(
                position, { 0.0f, yawFromDirection(incoming), 0.0f }, scale);
        } else if (hasPrevious && hasNext && turn < 0) {
            railRightPool_->Draw(
                position, { 0.0f, yawFromDirection(incoming), 0.0f }, scale);
        } else {
            railPool_->Draw(
                position, { 0.0f, yawFromDirection(outgoing), 0.0f }, scale);
        }
    }
}

void GameComponents::RailViewComponent::SetGridSize(float size) {
    gridSize_ = size;
}

void GameComponents::RailViewComponent::SetCenterPosition(const CoreEngine::Vector3& position) {
    transform_->Get().translate = position;
}

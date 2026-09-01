#include "pch.h"
#include "RailViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>
#include <list>

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
        lines.DrawLine({ x, 0.0f, z }, { nextX, 0.0f, nextZ }, { 1.0f, 0.0f, 0.0f }, 1.0f, true);
    }

    // 確定していないレールの座標を取得する
    const auto& railUndoStack = railPath_->GetRailUndoStack();
    for (size_t i = 0; i + 1 < railUndoStack.size(); ++i) {
        float x = static_cast<float>(railUndoStack[i].first) * gridSize_;
        float z = static_cast<float>(railUndoStack[i].second) * gridSize_;

        float nextX = static_cast<float>(railUndoStack[i + 1].first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack[i + 1].second) * gridSize_;

        // ラインを描画する
        lines.DrawLine({ x, 0.0f, z }, { nextX, 0.0f, nextZ }, { 1.0f, 1.0f, 1.0f }, 1.0f, true);
    }

    // 確定したレールと確定していないレールの間のラインを描画する
    if (!railMap.empty() && !railUndoStack.empty()) {
        float x = static_cast<float>(railMap.back().first) * gridSize_;
        float z = static_cast<float>(railMap.back().second) * gridSize_;
        float nextX = static_cast<float>(railUndoStack.front().first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack.front().second) * gridSize_;
        // ラインを描画する
        lines.DrawLine({ x, 0.0f, z }, { nextX, 0.0f, nextZ }, { 1.0f, 1.0f, 0.0f }, 1.0f, true);
    }
}

void GameComponents::RailViewComponent::SetGridSize(float size) {
    gridSize_ = size;
}

void GameComponents::RailViewComponent::SetCenterPosition(const CoreEngine::Vector3& position) {
    transform_->Get().translate = position;
}

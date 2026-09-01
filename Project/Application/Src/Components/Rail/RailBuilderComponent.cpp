#include "pch.h"
#include "RailBuilderComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "RailPathComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>

using namespace CoreEngine;

void GameComponents::RailBuilderComponent::Start() {
    transform_ = Sibling<TransformComponent>();

    if (!transform_ || !railPath_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "RailBuilderComponent: Transform または RailPath が未設定です");
        SetEnabled(false);
        return;
    }

    if (gridPosX_ < 0 || gridPosZ_ < 0 ||
        gridPosX_ >= static_cast<int32_t>(railPath_->GetMapSizeX()) ||
        gridPosZ_ >= static_cast<int32_t>(railPath_->GetMapSizeZ())) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "RailBuilderComponent: 初期位置が範囲外です ({}, {})",
            gridPosX_, gridPosZ_);
        SetEnabled(false);
        return;
    }

    SyncTransformToGrid();
}

void GameComponents::RailBuilderComponent::Update() {
    // TransformComponent がアタッチされていない場合は処理を中断する
    if (!transform_) {
        return;
    }
    // RailPathComponent がアタッチされていない場合は処理を中断する
    if (!railPath_) {
        return;
    }

    // ゲームオブジェクトのオーナーからエンジンシステムを取得し、入力マネージャーを取得する
    GameObject* owner = GetOwner();
    EngineSystem* engine =
        owner ? owner->GetEngineSystem() : nullptr;
    // ここで InputManager を取得する
    InputManager* inputManager =
        engine ? engine->GetService<InputManager>() : nullptr;
    // InputManager が存在しない場合は処理を中断する
    if (!inputManager) {
        return;
    }

    // 入力クエリを取得する
    const InputQuery& input = inputManager->GetQuery();

    // レールを撤去する（Undo）。移動入力とは同じフレームに処理しない
    if (input.IsActionTriggered(InputAction::Interact)) {
        const auto lastRail = railPath_->UndoLastRailPlacement();
        if (lastRail.first >= 0 && lastRail.second >= 0) {
            gridPosX_ = lastRail.first;
            gridPosZ_ = lastRail.second;
            SyncTransformToGrid();

            Logger::GetInstance().Infof(
                LogCategory::Game,
                "Rail removed; builder returned to ({}, {})",
                gridPosX_, gridPosZ_);
        }
        return;
    }
    // 連続削除のためのタイマー処理
    if (input.IsActionPressed(InputAction::Interact)) {
        undoPushTimer_ += Time::DeltaTime();
        // 連続削除ボタンを押し続けている時間が一定時間を超えた場合、連続削除を行う
        if (undoPushTimer_ >= undoPushMaxTime_) {
            // 連続削除の間隔タイマーを更新する
            if (undoIntervalTimer_ <= 0.0f) {
                const auto lastRail = railPath_->UndoLastRailPlacement();
                if (lastRail.first >= 0 && lastRail.second >= 0) {
                    gridPosX_ = lastRail.first;
                    gridPosZ_ = lastRail.second;
                    SyncTransformToGrid();
                    Logger::GetInstance().Infof(
                        LogCategory::Game,
                        "Rail removed; builder returned to ({}, {})",
                        gridPosX_, gridPosZ_);
                }
                undoIntervalTimer_ = undoInterval_;
                // 移動入力とは同じフレームに処理しない
                return;

            } else {
                undoIntervalTimer_ -= Time::DeltaTime();
            }
        }
    } else {
        undoPushTimer_ = 0.0f;
    }

    // X方向の移動量を計算する（右キー - 左キー）
    float moveX =
        static_cast<float>(input.IsActionTriggered(InputAction::MoveRight)) -
        static_cast<float>(input.IsActionTriggered(InputAction::MoveLeft));

    // Z方向の移動量を計算する（前キー - 後キー）
    float moveZ =
        static_cast<float>(input.IsActionTriggered(InputAction::MoveForward)) -
        static_cast<float>(input.IsActionTriggered(InputAction::MoveBack));

    // 移動量がゼロの場合は処理を中断する
    if (moveX == 0.0f && moveZ == 0.0f) {
        return;
    }

    // 優先方向以外の移動を無効化する
    if (HorizontalPrioritize) {// 水平方向優先
        if(std::abs(moveX)> 0.0f) {
            moveZ = 0.0f;
        }
    } else {// 垂直方向優先
        if (std::abs(moveZ) > 0.0f) {
            moveX = 0.0f;
        }
    }

    // Transform ではなく論理グリッド座標を先に更新する
    const int32_t nextX = gridPosX_ + static_cast<int32_t>(moveX);
    const int32_t nextZ = gridPosZ_ + static_cast<int32_t>(moveZ);

    if (nextX < 0 || nextZ < 0 ||
        nextZ >= static_cast<int32_t>(railPath_->GetMapSizeZ())) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailBuilder: 移動先が範囲外です ({}, {})",
            nextX, nextZ);
        return;
    }

    // 既にレールがあるマスへは移動しない
    auto& railMap = railPath_->GetRailMap();
    for (auto& rail : railMap) {
        if (rail.first == nextX && rail.second == nextZ) {
            Logger::GetInstance().Infof(
                LogCategory::Game,
                "RailBuilder: 既設レールのため移動を中止しました ({}, {})",
                nextX, nextZ);
            return;
        }
    }
    auto& railUndoStack = railPath_->GetRailUndoStack();
    for (auto& rail : railUndoStack) {
        if (rail.first == nextX && rail.second == nextZ) {
            Logger::GetInstance().Infof(
                LogCategory::Game,
                "RailBuilder: 既設レールのため移動を中止しました ({}, {})",
                nextX, nextZ);
            return;
        }
    }

    gridPosX_ = nextX;
    gridPosZ_ = nextZ;
    railPath_->PlaceRail(gridPosX_, gridPosZ_);
    SyncTransformToGrid();

    Logger::GetInstance().Infof(
        LogCategory::Game,
        "Rail placed at ({}, {})",
        gridPosX_, gridPosZ_);
}

void GameComponents::RailBuilderComponent::SyncTransformToGrid() {
    if (!transform_) {
        return;
    }

    transform_->Get().translate.x = static_cast<float>(gridPosX_) * gridSize_;
    transform_->Get().translate.z = static_cast<float>(gridPosZ_) * gridSize_;
}

void GameComponents::RailBuilderComponent::SetGridSize(float size) {
    gridSize_ = size;  
}

void GameComponents::RailBuilderComponent::SetHorizontalPrioritize(bool prioritize) {
    HorizontalPrioritize = prioritize;
}   

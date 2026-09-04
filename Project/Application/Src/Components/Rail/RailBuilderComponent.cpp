#include "pch.h"
#include "RailBuilderComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "RailPathComponent.h"
#include "RailResourceManagerComponent.h"
#include "Components/Building/MapGeneratorComponent.h"
#include "Components/Train/TrainMovementComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

using namespace CoreEngine;

void GameComponents::RailBuilderComponent::Start() {
    transform_ = Sibling<TransformComponent>();

    if (!transform_ || !railPath_ || !resourceManager_ ||
        !mapGenerator_ || !trainMovement_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "RailBuilderComponent: 必要なコンポーネントが未設定です");
        SetEnabled(false);
        return;
    }

    if (gridPosX_ < 0 || gridPosZ_ < 0 ||
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
    // RailResourceManagerComponent がアタッチされていない場合は処理を中断する
    if (!resourceManager_) {
        return;
    }

    // タイマーを更新する
    timer_ += Time::DeltaTime();

    // TransformComponent のスケールと回転を更新する
    transform_->Get().scale = { 1.0f, 0.8f + (sinf(timer_ * 5.0f) * 0.2f), 1.0f };
    transform_->Get().rotate.y = timer_ * 2.0f;

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
        TryUndoLastRail();
        return;
    }
    // 連続削除のためのタイマー処理
    if (input.IsActionPressed(InputAction::Interact)) {
        undoPushTimer_ += Time::DeltaTime();
        // 連続削除ボタンを押し続けている時間が一定時間を超えた場合、連続削除を行う
        if (undoPushTimer_ >= undoPushMaxTime_) {
            // 連続削除の間隔タイマーを更新する
            if (undoIntervalTimer_ <= 0.0f) {
                TryUndoLastRail();
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

    mapGenerator_->CreateToX(static_cast<std::size_t>(nextX) + 1);
    const MapChipType mapChip = mapGenerator_->GetMapChip(
        static_cast<std::size_t>(nextX), static_cast<std::size_t>(nextZ));

    // Void はマップ外も含む非建設マスとして扱う。
    if (mapChip == MapChipType::Void) {
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailBuilder: Voidチップのためレールを設置できません ({}, {})",
            nextX, nextZ);
        return;
    }

    // レールが0本なら、報酬マスであっても新しいレールは設置できない。
    if (resourceManager_->GetResourceCount() == 0) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailBuilder: レールがありません");
        return;
    }

    uint32_t resourceCost = 0;
    if (mapChip == MapChipType::Ground) {
        resourceCost = 1;
    } else if (mapChip == MapChipType::Water) {
        resourceCost = 2;
    }

    if (!resourceManager_->HasEnoughResource(resourceCost)) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "RailBuilder: レールが不足しています (必要={}, 所持={})",
            resourceCost, resourceManager_->GetResourceCount());
        return;
    }

    OnBuildSE_();
    if (!resourceManager_->UseResource(resourceCost)) {
        return;
    }

    if (!railPath_->PlaceRail(nextX, nextZ, resourceCost)) {
        resourceManager_->AddResource(resourceCost);
        return;
    }

    gridPosX_ = nextX;
    gridPosZ_ = nextZ;
    SyncTransformToGrid();

    if (mapChip == MapChipType::Station) {
        const uint32_t reward = CalculateSpeedReward(15);
        resourceManager_->AddResource(reward);
        railPath_->ConfirmAllPendingRailPlacements();
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailBuilder: 駅に到達しました (報酬={}, 駅までのレールを確定)",
            reward);
    } else if (mapChip == MapChipType::Resource) {
        const uint32_t reward = CalculateSpeedReward(5);
        resourceManager_->AddResource(reward);
        // リソース床は一度だけ取得できるよう、通常のGroundへ戻す。
        mapGenerator_->SetMapChip(
            static_cast<std::size_t>(gridPosX_),
            static_cast<std::size_t>(gridPosZ_),
            MapChipType::Ground);
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailBuilder: リソースを取得しました (報酬={})",
            reward);
    }

    Logger::GetInstance().Infof(
        LogCategory::Game,
        "Rail placed at ({}, {})",
        gridPosX_, gridPosZ_);
}

bool GameComponents::RailBuilderComponent::TryUndoLastRail() {
    const RailUndoResult undo = railPath_->UndoLastRailPlacement();
    if (!undo.succeeded) {
        return false;
    }

    resourceManager_->AddResource(undo.refundAmount);
    gridPosX_ = undo.builderPosition.first;
    gridPosZ_ = undo.builderPosition.second;
    SyncTransformToGrid();

    Logger::GetInstance().Infof(
        LogCategory::Game,
        "Rail removed at ({}, {}); builder returned to ({}, {}), refund={}",
        undo.removedPosition.first, undo.removedPosition.second,
        gridPosX_, gridPosZ_, undo.refundAmount);

    OnUndoSE_();
    return true;
}

uint32_t GameComponents::RailBuilderComponent::CalculateSpeedReward(
    uint32_t baseAmount) const {
    const float speedRatio = trainMovement_
        ? std::max(trainMovement_->GetSpeedRatio(), 1.0f)
        : 1.0f;

    float speedRatioClamped = std::clamp(speedRatio, 1.0f, 2.0f);

    return static_cast<uint32_t>(
        std::floor(static_cast<float>(baseAmount) * speedRatioClamped));
}

void GameComponents::RailBuilderComponent::SyncTransformToGrid() {
    if (!transform_) {
        return;
    }

    transform_->Get().translate.x = static_cast<float>(gridPosX_) * gridSize_;
    transform_->Get().translate.z = static_cast<float>(gridPosZ_) * gridSize_;

    transform_->Get().translate.y = 1.0f;
}

void GameComponents::RailBuilderComponent::SetGridSize(float size) {
    gridSize_ = size;  
}

void GameComponents::RailBuilderComponent::SetHorizontalPrioritize(bool prioritize) {
    HorizontalPrioritize = prioritize;
}   

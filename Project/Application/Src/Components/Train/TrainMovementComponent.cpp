#include "pch.h"
#include "TrainMovementComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

using namespace CoreEngine;

void GameComponents::TrainMovementComponent::Start() {
    transform_ = Sibling<TransformComponent>();
    // RailPathComponent がアタッチされていない場合は処理を中断する
    if (!transform_ || !railPath_ || !gameManager_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "TrainMovementComponent: Transform、RailPath または GameManager が未設定です");
        SetEnabled(false);
        return;
    }

    // 初期位置が RailPathComponent の範囲外であればエラーを出して無効化する
    if (gridX_ < 0 || gridZ_ < 0 ||
        gridZ_ >= static_cast<int32_t>(railPath_->GetMapSizeZ())) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "TrainMovementComponent: 初期位置が範囲外です ({}, {})",
            gridX_, gridZ_);
        SetEnabled(false);
        return;
    }

    // 初期位置を TransformComponent に反映する
    transform_->Get().translate.x = static_cast<float>(gridX_) * gridSize_;
    transform_->Get().translate.z = static_cast<float>(gridZ_) * gridSize_;
}

void GameComponents::TrainMovementComponent::Update() {
    if (!transform_ || !railPath_ || isGameOver_) {
        return;
    }

    // 発車前は、プレイヤーが未確定レールを指定マス敷くまで待機する。
    if (!hasStarted_) {
        if (railPath_->GetUnconfirmedRailCount() < kRequiredRailCount) {
            return;
        }
        hasStarted_ = true;
    }

    const float deltaTime = Time::DeltaTime();
    if (deltaTime <= 0.0f) {
        return;
    }

    // 発車後は走行時間に応じて加速する。
    moveSpeed_ += speedUpFactor_ * deltaTime;

    // 移動量を計算する前に進行方向を確定し、曲がり角なら減速を反映する。
    if (!isMoving_ && !BeginNextSegment()) {
        NotifyGameOver();
        return;
    }

    // DeltaTime に応じて移動進捗を加算する。
    float remainingProgress = moveSpeed_ * deltaTime;
    if (remainingProgress <= 0.0f) {
        return;
    }

    // 大きな DeltaTime でも目的地を飛び越さないよう、余った進捗を次のマスへ持ち越す。
    while (remainingProgress > 0.0f && !isGameOver_) {
        if (!isMoving_) {
            const float speedBeforeTurn = moveSpeed_;
            if (!BeginNextSegment()) {
                NotifyGameOver();
                break;
            }

            // 同じフレーム内で曲がった場合も、残りの移動量へ減速を反映する。
            if (speedBeforeTurn > 0.0f && moveSpeed_ < speedBeforeTurn) {
                remainingProgress *= moveSpeed_ / speedBeforeTurn;
            }
        }

        const float progressToDestination = 1.0f - movementProgress_;
        const float appliedProgress = std::min(remainingProgress, progressToDestination);
        movementProgress_ += appliedProgress;
        remainingProgress -= appliedProgress;
        SyncTransformToProgress();

        if (movementProgress_ < 1.0f) {
            break;
        }

        // 誤差を残さず、到着したマスの中央へ固定する。
        gridX_ = destinationGridX_;
        gridZ_ = destinationGridZ_;
        movementProgress_ = 0.0f;
        isMoving_ = false;
        transform_->Get().translate.x = static_cast<float>(gridX_) * gridSize_;
        transform_->Get().translate.z = static_cast<float>(gridZ_) * gridSize_;

        // 発車後に終端へ到着した時点でゲームオーバーにする。
        if (railPath_->GetUnconfirmedRailCount() == 0) {
            NotifyGameOver();
        }
    }
}

void GameComponents::TrainMovementComponent::NotifyGameOver() {
    if (isGameOver_) {
        return;
    }

    isGameOver_ = true;
    if (gameManager_) {
        gameManager_->RequestGameOver();
    }
}

bool GameComponents::TrainMovementComponent::IsGameOver() const {
    return isGameOver_;
}

void GameComponents::TrainMovementComponent::SetGridSize(float size) {
    if (size > 0.0f) {
        gridSize_ = size;
    }
}

bool GameComponents::TrainMovementComponent::BeginNextSegment() {
    std::pair<int32_t, int32_t> destination{};
    if (!railPath_->TryGetNextUnconfirmedRail(destination)) {
        return false;
    }

    const int32_t deltaX = destination.first - gridX_;
    const int32_t deltaZ = destination.second - gridZ_;
    if (std::abs(deltaX) + std::abs(deltaZ) != 1) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "TrainMovementComponent: レールが連続していません ({}, {}) -> ({}, {})",
            gridX_, gridZ_, destination.first, destination.second);
        return false;
    }

    // 確定でキューから消える前に、移動先をコンポーネント内へ保存する。
    destinationGridX_ = destination.first;
    destinationGridZ_ = destination.second;
    if (!railPath_->ConfirmNextRailPlacement()) {
        return false;
    }

    movementProgress_ = 0.0f;
    isMoving_ = true;
    UpdateRotation();
    return true;
}

void GameComponents::TrainMovementComponent::SyncTransformToProgress() {
    // 移動中のマスの中央から、目的地のマスの中央までの線形補間
    const float startX = static_cast<float>(gridX_) * gridSize_;
    const float startZ = static_cast<float>(gridZ_) * gridSize_;
    const float destinationX = static_cast<float>(destinationGridX_) * gridSize_;
    const float destinationZ = static_cast<float>(destinationGridZ_) * gridSize_;
    // 進捗に応じて TransformComponent の位置を更新する
    transform_->Get().translate.x =
        startX + (destinationX - startX) * movementProgress_;
    transform_->Get().translate.z =
        startZ + (destinationZ - startZ) * movementProgress_;

    transform_->Get().translate.y = 1.0f;
}

void GameComponents::TrainMovementComponent::UpdateRotation() {
    // 進行方向に応じて Y 軸回転を設定する
    const int32_t deltaX = destinationGridX_ - gridX_;
    const int32_t deltaZ = destinationGridZ_ - gridZ_;
    const float oldRotationY = transform_->Get().rotate.y;

    // 進行方向が X 軸正方向なら 90 度、X 軸負方向なら -90 度、Z 軸負方向なら 180 度、Z 軸正方向なら 0 度
    if (deltaX > 0) {
        transform_->Get().rotate.y = std::numbers::pi_v<float> * 0.5f;
    } else if (deltaX < 0) {
        transform_->Get().rotate.y = -std::numbers::pi_v<float> * 0.5f;
    } else if (deltaZ < 0) {
        transform_->Get().rotate.y = std::numbers::pi_v<float>;
    } else {
        transform_->Get().rotate.y = 0.0f;
    }

    // 初回の進行方向は曲がり角として扱わない。
    // 2区間目以降で向きが変わった場合だけ速度を半分にする。
    const float newRotationY = transform_->Get().rotate.y;
    if (hasDirection_ && oldRotationY != newRotationY) {
        moveSpeed_ = std::max(moveSpeed_ * 0.5f, minMoveSpeed_);

        Logger::GetInstance().Infof(
            LogCategory::Game,
            "TrainMovementComponent: 方向転換により速度を落としました。新しい速度: {}",
            moveSpeed_);
    }

    hasDirection_ = true;
}

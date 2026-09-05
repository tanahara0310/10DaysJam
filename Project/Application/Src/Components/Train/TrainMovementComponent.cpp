#include "pch.h"
#include "TrainMovementComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Components/GameCore/HungerComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

namespace {
    constexpr float kCompletedRailSpeedMultiplier = 10.0f;
}

json GameComponents::TrainMovementComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "initialMoveSpeed", initialMoveSpeed_ },
        { "initialGridX", initialGridX_ },
        { "initialGridZ", initialGridZ_ },
        { "speedUpFactor", speedUpFactor_ },
        { "minMoveSpeed", minMoveSpeed_ },
        { "turnSlowdownFactor", turnSlowdownFactor_ },
        { "completedRailPauseDuration", completedRailPauseDuration_ },
        { "boostJumpHeight", boostJumpHeight_ },
        { "boostJumpDuration", boostJumpDuration_ },
        { "rockThrowJumpHeight", rockThrowJumpHeight_ },
        { "rockThrowJumpDuration", rockThrowJumpDuration_ },
        { "trainHeight", trainHeight_ },
        { "requiredRailCount", requiredRailCount_ }
    };
}

void GameComponents::TrainMovementComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    initialMoveSpeed_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "initialMoveSpeed", initialMoveSpeed_));
    initialGridX_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridX", initialGridX_));
    initialGridZ_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridZ", initialGridZ_));
    speedUpFactor_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "speedUpFactor", speedUpFactor_));
    minMoveSpeed_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "minMoveSpeed", minMoveSpeed_));
    turnSlowdownFactor_ = std::clamp(
        JsonManager::SafeGet<float>(j, "turnSlowdownFactor", turnSlowdownFactor_), 0.0f, 1.0f);
    completedRailPauseDuration_ = std::max(0.0f,
        JsonManager::SafeGet<float>(
            j, "completedRailPauseDuration", completedRailPauseDuration_));
    boostJumpHeight_ = std::max(
        0.0f, JsonManager::SafeGet<float>(j, "boostJumpHeight", boostJumpHeight_));
    boostJumpDuration_ = std::max(
        0.0f, JsonManager::SafeGet<float>(j, "boostJumpDuration", boostJumpDuration_));
    rockThrowJumpHeight_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "rockThrowJumpHeight", rockThrowJumpHeight_));
    rockThrowJumpDuration_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "rockThrowJumpDuration", rockThrowJumpDuration_));
    trainHeight_ = JsonManager::SafeGet<float>(j, "trainHeight", trainHeight_);
    requiredRailCount_ = std::max<std::size_t>(1,
        JsonManager::SafeGet<std::size_t>(j, "requiredRailCount", requiredRailCount_));
    moveSpeed_ = std::max(initialMoveSpeed_, minMoveSpeed_);
    gridX_ = initialGridX_;
    gridZ_ = initialGridZ_;
}

#ifdef USE_IMGUI
bool GameComponents::TrainMovementComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    if (ImGui::DragFloat("初期速度", &initialMoveSpeed_, 0.01f, 0.0f, 20.0f)) {
        moveSpeed_ = std::max(initialMoveSpeed_, minMoveSpeed_);
        changed = true;
    }
    changed |= ImGui::DragFloat("加速係数", &speedUpFactor_, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("最低速度", &minMoveSpeed_, 0.01f, 0.0f, 20.0f);
    changed |= ImGui::SliderFloat("カーブ減速倍率", &turnSlowdownFactor_, 0.0f, 1.0f);
    changed |= ImGui::DragFloat(
        "確定レール発進待機", &completedRailPauseDuration_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(
        "確定レール待機ジャンプ高さ", &boostJumpHeight_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(
        "確定レール待機ジャンプ時間", &boostJumpDuration_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat(
        "投石ジャンプ高さ", &rockThrowJumpHeight_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(
        "投石ジャンプ時間", &rockThrowJumpDuration_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("列車の高さ", &trainHeight_, 0.05f, -20.0f, 20.0f);
    int required = static_cast<int>(requiredRailCount_);
    if (ImGui::DragInt("発車に必要なレール数", &required, 1.0f, 1, 100)) {
        requiredRailCount_ = static_cast<std::size_t>(std::max(required, 1));
        changed = true;
    }
    changed |= ImGui::DragInt("初期X", &initialGridX_, 1.0f, 0, 500);
    changed |= ImGui::DragInt("初期Z", &initialGridZ_, 1.0f, 0, 100);
    ImGui::TextDisabled("現在速度: %.3f", moveSpeed_);
    return changed;
}
#endif

void GameComponents::TrainMovementComponent::Start() {
    transform_ = Sibling<TransformComponent>();
    // RailPathComponent がアタッチされていない場合は処理を中断する
    if (!transform_ || !railPath_ || !gameManager_ || !hunger_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "TrainMovementComponent: Transform、RailPath、GameManager または Hunger が未設定です");
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
    transform_->Get().translate.y = trainHeight_;
    transform_->Get().translate.z = static_cast<float>(gridZ_) * gridSize_;
}

void GameComponents::TrainMovementComponent::Update() {
    if (!transform_ || !railPath_ || isGameOver_) {
        return;
    }

    const float deltaTime = Time::DeltaTime();
    UpdateRockThrowJump(deltaTime);

    // 投石キューが空になるまでは移動せず、その場で投石ジャンプだけ再生する。
    if (isPausedForRockBreak_) {
        transform_->Get().translate.y = trainHeight_ + GetRockThrowJumpOffset();
        return;
    }

    // 発車前は、プレイヤーが未確定レールを指定マス敷くまで待機する。
    if (!hasStarted_) {
        if (railPath_->GetUnconfirmedRailCount() < requiredRailCount_) {
            return;
        }
        hasStarted_ = true;
    }

    if (deltaTime <= 0.0f) {
        return;
    }

    // 発車後は走行時間に応じて加速する。
    moveSpeed_ += speedUpFactor_ * deltaTime;

    // 確定レールへ切り替わった直後は、その場で指定時間だけ待機する。
    float movementDeltaTime = deltaTime;
    if (completedRailPauseRemaining_ > 0.0f) {
        const float pauseDeltaTime = std::min(
            movementDeltaTime, completedRailPauseRemaining_);
        completedRailPauseRemaining_ -= pauseDeltaTime;
        movementDeltaTime -= pauseDeltaTime;
        UpdateBoostJump(pauseDeltaTime);
        transform_->Get().translate.y = trainHeight_ + GetBoostJumpOffset();

        if (completedRailPauseRemaining_ > 0.0f) {
            return;
        }

        // 高速移動へ入る前に必ず着地させる。
        isBoostJumping_ = false;
        transform_->Get().translate.y = trainHeight_;
    }

    // 移動量を計算する前に進行方向を確定し、曲がり角なら減速を反映する。
    if (!isMoving_ && !BeginNextSegment()) {
        NotifyGameOver();
        return;
    }
    if (completedRailPauseRemaining_ > 0.0f) {
        return;
    }

    // DeltaTime に応じて移動進捗を加算する。
    float remainingProgress = moveSpeed_ * movementDeltaTime *
        (isMovingOnCompletedRail_ ? kCompletedRailSpeedMultiplier : 1.0f);
    if (remainingProgress <= 0.0f) {
        return;
    }

    // 大きな DeltaTime でも目的地を飛び越さないよう、余った進捗を次のマスへ持ち越す。
    while (remainingProgress > 0.0f && !isGameOver_) {
        if (!isMoving_) {
            const float effectiveSpeedBeforeSegment = moveSpeed_ *
                (isMovingOnCompletedRail_ ? kCompletedRailSpeedMultiplier : 1.0f);
            if (!BeginNextSegment()) {
                NotifyGameOver();
                break;
            }
            if (completedRailPauseRemaining_ > 0.0f) {
                break;
            }

            // 同じフレーム内で区間が切り替わった場合も、完成状態の違いと
            // カーブ減速を残りの移動量へ反映する。
            const float effectiveSpeedAfterSegment = moveSpeed_ *
                (isMovingOnCompletedRail_ ? kCompletedRailSpeedMultiplier : 1.0f);
            if (effectiveSpeedBeforeSegment > 0.0f &&
                effectiveSpeedAfterSegment > 0.0f) {
                remainingProgress *= effectiveSpeedAfterSegment /
                    effectiveSpeedBeforeSegment;
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

        hunger_->OnTrainEnteredCell(gridX_, gridZ_);

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
    isBoostJumping_ = false;
    if (transform_) {
        transform_->Get().translate.y = trainHeight_;
    }
    if (gameManager_) {
        gameManager_->RequestGameOver();
    }
}

bool GameComponents::TrainMovementComponent::IsGameOver() const {
    return isGameOver_;
}

Vector3 GameComponents::TrainMovementComponent::GetWorldPosition() const {
    if (transform_) {
        return transform_->Get().translate;
    }
    return {
        static_cast<float>(gridX_) * gridSize_,
        trainHeight_,
        static_cast<float>(gridZ_) * gridSize_
    };
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

    // ConfirmNextRailPlacement() の前に判定することで、今回新しく確定される
    // レールではなく、すでに完成していたレールだけを加速対象にする。
    const bool wasMovingOnCompletedRail = isMovingOnCompletedRail_;
    const auto& completedRails = railPath_->GetRailMap();
    isMovingOnCompletedRail_ = std::find(
        completedRails.begin(), completedRails.end(), destination) != completedRails.end();

    // 確定でキューから消える前に、移動先をコンポーネント内へ保存する。
    destinationGridX_ = destination.first;
    destinationGridZ_ = destination.second;
    if (!railPath_->ConfirmNextRailPlacement()) {
        return false;
    }

    // 連続する確定レールでは毎マス停止せず、通常区間から切り替わる瞬間だけ待機する。
    if (isMovingOnCompletedRail_ && !wasMovingOnCompletedRail) {
        completedRailPauseRemaining_ = completedRailPauseDuration_;
        boostJumpElapsed_ = 0.0f;
        isBoostJumping_ = boostJumpDuration_ > 0.0f && boostJumpHeight_ > 0.0f;
    }

    hunger_->StartDraining();
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

    transform_->Get().translate.y = trainHeight_ + GetBoostJumpOffset();
}

void GameComponents::TrainMovementComponent::UpdateBoostJump(float deltaTime) {
    if (!isBoostJumping_) {
        return;
    }

    boostJumpElapsed_ += std::max(deltaTime, 0.0f);
    if (boostJumpElapsed_ >= boostJumpDuration_) {
        boostJumpElapsed_ = boostJumpDuration_;
        isBoostJumping_ = false;
    }
}

float GameComponents::TrainMovementComponent::GetBoostJumpOffset() const {
    if (!isBoostJumping_ || boostJumpDuration_ <= 0.0f) {
        return 0.0f;
    }

    const float progress = std::clamp(boostJumpElapsed_ / boostJumpDuration_, 0.0f, 1.0f);
    // 0→頂点→0となる放物線。確定レール前の停止中に飛び上がる。
    return boostJumpHeight_ * 4.0f * progress * (1.0f - progress);
}

void GameComponents::TrainMovementComponent::PlayRockThrowJump() {
    rockThrowJumpElapsed_ = 0.0f;
    isRockThrowJumping_ = rockThrowJumpDuration_ > 0.0f && rockThrowJumpHeight_ > 0.0f;
}

void GameComponents::TrainMovementComponent::UpdateRockThrowJump(float deltaTime) {
    if (!isRockThrowJumping_) {
        return;
    }

    rockThrowJumpElapsed_ += std::max(deltaTime, 0.0f);
    if (rockThrowJumpElapsed_ >= rockThrowJumpDuration_) {
        rockThrowJumpElapsed_ = rockThrowJumpDuration_;
        isRockThrowJumping_ = false;
    }
}

float GameComponents::TrainMovementComponent::GetRockThrowJumpOffset() const {
    if (!isRockThrowJumping_ || rockThrowJumpDuration_ <= 0.0f) {
        return 0.0f;
    }

    const float progress = std::clamp(
        rockThrowJumpElapsed_ / rockThrowJumpDuration_, 0.0f, 1.0f);
    return rockThrowJumpHeight_ * 4.0f * progress * (1.0f - progress);
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
        moveSpeed_ = std::max(moveSpeed_ * turnSlowdownFactor_, minMoveSpeed_);

        Logger::GetInstance().Infof(
            LogCategory::Game,
            "TrainMovementComponent: 方向転換により速度を落としました。新しい速度: {}",
            moveSpeed_);
    }

    hasDirection_ = true;
}

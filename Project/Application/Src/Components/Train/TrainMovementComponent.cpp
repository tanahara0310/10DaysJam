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

json GameComponents::TrainMovementComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "initialMoveSpeed", initialMoveSpeed_ },
        { "initialGridX", initialGridX_ },
        { "initialGridZ", initialGridZ_ },
        { "minMoveSpeed", minMoveSpeed_ },
        { "speedIncreaseIntervalBlocks", speedIncreaseIntervalBlocks_ },
        { "speedIncreaseAmount", speedIncreaseAmount_ },
        { "maximumMoveSpeed", maximumMoveSpeed_ },
        { "stationSlowdownMultiplier", stationSlowdownMultiplier_ },
        { "stationSlowdownDuration", stationSlowdownDuration_ },
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
    minMoveSpeed_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "minMoveSpeed", minMoveSpeed_));
    speedIncreaseIntervalBlocks_ = std::max<std::size_t>(1,
        JsonManager::SafeGet<std::size_t>(j, "speedIncreaseIntervalBlocks", speedIncreaseIntervalBlocks_));
    speedIncreaseAmount_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "speedIncreaseAmount", speedIncreaseAmount_));
    maximumMoveSpeed_ = std::max(initialMoveSpeed_,
        JsonManager::SafeGet<float>(j, "maximumMoveSpeed", maximumMoveSpeed_));
    stationSlowdownMultiplier_ = std::clamp(
        JsonManager::SafeGet<float>(j, "stationSlowdownMultiplier", stationSlowdownMultiplier_),
        0.0f, 1.0f);
    stationSlowdownDuration_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "stationSlowdownDuration", stationSlowdownDuration_));
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

    ImGui::SeparatorText("走行");
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    if (ImGui::DragFloat("初期速度", &initialMoveSpeed_, 0.01f, 0.0f, 20.0f)) {
        moveSpeed_ = std::max(initialMoveSpeed_, minMoveSpeed_);
        changed = true;
    }
    changed |= ImGui::DragFloat("最低速度", &minMoveSpeed_, 0.01f, 0.0f, 20.0f);
    int speedInterval = static_cast<int>(speedIncreaseIntervalBlocks_);
    if (ImGui::DragInt("速度上昇間隔（ブロック）", &speedInterval, 1.0f, 1, 1000)) {
        speedIncreaseIntervalBlocks_ = static_cast<std::size_t>(std::max(speedInterval, 1));
        changed = true;
    }
    changed |= ImGui::DragFloat("段階ごとの速度上昇量", &speedIncreaseAmount_, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("最高速度", &maximumMoveSpeed_, 0.01f, 0.01f, 100.0f);
    changed |= ImGui::SliderFloat("駅減速倍率", &stationSlowdownMultiplier_, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("駅減速時間", &stationSlowdownDuration_, 0.05f, 0.0f, 30.0f);
    changed |= ImGui::DragFloat(
        "投石ジャンプ高さ", &rockThrowJumpHeight_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(
        "投石ジャンプ時間", &rockThrowJumpDuration_, 0.01f, 0.0f, 5.0f);

    ImGui::SeparatorText("配置");
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

    const int32_t extendedBlocks = std::max(
        0, railPath_->GetFurthestRailX() - initialGridX_);
    const float dynamicMinimum = initialMoveSpeed_ +
        static_cast<float>(extendedBlocks / static_cast<int32_t>(speedIncreaseIntervalBlocks_)) *
        speedIncreaseAmount_;
    minMoveSpeed_ = std::min(dynamicMinimum, maximumMoveSpeed_);
    stationSlowdownRemaining_ = std::max(0.0f, stationSlowdownRemaining_ - deltaTime);
    moveSpeed_ = minMoveSpeed_ *
        (stationSlowdownRemaining_ > 0.0f ? stationSlowdownMultiplier_ : 1.0f);

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
            if (!BeginNextSegment()) {
                NotifyGameOver();
                break;
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

        horizontalProgressBlocks_ = std::max(horizontalProgressBlocks_,
            static_cast<uint32_t>(std::max(0, gridX_ - initialGridX_)));
        if (hunger_->OnTrainEnteredCell(gridX_, gridZ_)) {
            stationSlowdownRemaining_ = stationSlowdownDuration_;
            moveSpeed_ = minMoveSpeed_ * stationSlowdownMultiplier_;
        }

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

    transform_->Get().translate.y = trainHeight_ + GetRockThrowJumpOffset();
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

}

#include "pch.h"
#include "TrainMovementComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Components/GameCore/HungerComponent.h"
#include "Components/Utility/BlockModelLayout.h"
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
        { "minimumSpeedIncreasePerRail", minimumSpeedIncreasePerRail_ },
        { "acceleration", acceleration_ },
        { "maximumMoveSpeed", maximumMoveSpeed_ },
        { "rockThrowJumpHeight", rockThrowJumpHeight_ },
        { "rockThrowJumpDuration", rockThrowJumpDuration_ },
        { "requiredRailCount", requiredRailCount_ }
    };
}

void GameComponents::TrainMovementComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    initialMoveSpeed_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "initialMoveSpeed", initialMoveSpeed_));
    initialGridX_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridX", initialGridX_));
    initialGridZ_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridZ", initialGridZ_));
    minimumSpeedIncreasePerRail_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "minimumSpeedIncreasePerRail", minimumSpeedIncreasePerRail_));
    acceleration_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "acceleration", acceleration_));
    maximumMoveSpeed_ = std::max(initialMoveSpeed_,
        JsonManager::SafeGet<float>(j, "maximumMoveSpeed", maximumMoveSpeed_));
    rockThrowJumpHeight_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "rockThrowJumpHeight", rockThrowJumpHeight_));
    rockThrowJumpDuration_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "rockThrowJumpDuration", rockThrowJumpDuration_));
    requiredRailCount_ = std::max<std::size_t>(1,
        JsonManager::SafeGet<std::size_t>(j, "requiredRailCount", requiredRailCount_));
    // 最低速度は敷設レール数から毎フレーム再計算するため、読込直後は基準値に戻す。
    minMoveSpeed_ = initialMoveSpeed_;
    moveSpeed_ = initialMoveSpeed_;
    gridX_ = initialGridX_;
    gridZ_ = initialGridZ_;
}

#ifdef USE_IMGUI
bool GameComponents::TrainMovementComponent::DrawInspector() {
    bool changed = false;

    ImGui::SeparatorText("走行");
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    if (ImGui::DragFloat("基準最低速度（初期・駅リセット）", &initialMoveSpeed_, 0.01f, 0.0f, 20.0f)) {
        maximumMoveSpeed_ = std::max(maximumMoveSpeed_, initialMoveSpeed_);
        moveSpeed_ = std::max(moveSpeed_, initialMoveSpeed_);
        changed = true;
    }
    changed |= ImGui::DragFloat(
        "最低速度の増加量（レール1マス）", &minimumSpeedIncreasePerRail_, 0.001f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("加速度（速度/秒）", &acceleration_, 0.01f, 0.0f, 20.0f);
    changed |= ImGui::DragFloat("最高速度", &maximumMoveSpeed_, 0.01f, 0.01f, 100.0f);
    maximumMoveSpeed_ = std::max(maximumMoveSpeed_, initialMoveSpeed_);
    moveSpeed_ = std::min(moveSpeed_, maximumMoveSpeed_);
    ImGui::TextDisabled("現在の最低速度: %.3f", minMoveSpeed_);
    changed |= ImGui::DragFloat(
        "投石ジャンプ高さ", &rockThrowJumpHeight_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(
        "投石ジャンプ時間", &rockThrowJumpDuration_, 0.01f, 0.0f, 5.0f);

    ImGui::SeparatorText("配置");
    ImGui::TextDisabled("列車の接地高さ: %.3f", BlockModelLayout::GetRailTopHeight(gridSize_));
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
    const float modelScale = BlockModelLayout::GetScale(gridSize_);
    transform_->Get().scale = { modelScale, modelScale, modelScale };
    transform_->Get().translate.x = static_cast<float>(gridX_) * gridSize_;
    transform_->Get().translate.y = BlockModelLayout::GetRailTopHeight(gridSize_);
    transform_->Get().translate.z = static_cast<float>(gridZ_) * gridSize_;
    traveledCells_.clear();
    traveledCells_.emplace_back(gridX_, gridZ_);
    pendingStationSteps_.clear();
    traveledBlockCount_ = 0;
}

void GameComponents::TrainMovementComponent::Update() {
    if (!transform_ || !railPath_ || isGameOver_) {
        return;
    }

    const float deltaTime = Time::DeltaTime();
    UpdateRockThrowJump(deltaTime);
    const float modelScale = BlockModelLayout::GetScale(gridSize_);
    transform_->Get().scale = { modelScale, modelScale, modelScale };
    transform_->Get().translate.y =
        BlockModelLayout::GetRailTopHeight(gridSize_) + GetRockThrowJumpOffset();
    SyncCarriageTransforms();

    // 投石キューが空になるまでは移動せず、その場で投石ジャンプだけ再生する。
    if (isPausedForRockBreak_) {
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

    const std::size_t laidRailCount = railPath_->GetLaidRailCount();
    const float dynamicMinimum = initialMoveSpeed_ +
        static_cast<float>(laidRailCount) * minimumSpeedIncreasePerRail_;
    minMoveSpeed_ = std::min(dynamicMinimum, maximumMoveSpeed_);
    // 駅で最低速度へ戻した後、毎秒の加速度で最高速度まで徐々に加速する。
    moveSpeed_ = std::clamp(
        moveSpeed_ + acceleration_ * deltaTime,
        minMoveSpeed_, maximumMoveSpeed_);

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
        const bool stationActivated = hunger_->OnTrainEnteredCell(gridX_, gridZ_);
        if (stationActivated) {
            // 駅では一時減速せず、現在の最低速度を次の加速の開始速度にする。
            moveSpeed_ = minMoveSpeed_;
        }
        ProcessCarriageArrival(stationActivated);

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
        transform_->Get().translate.y = BlockModelLayout::GetRailTopHeight(gridSize_);
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
        BlockModelLayout::GetRailTopHeight(gridSize_),
        static_cast<float>(gridZ_) * gridSize_
    };
}

float GameComponents::TrainMovementComponent::GetCursorHeightOffsetAt(
    float worldX, float worldZ) const {
    float heightOffset = 0.0f;
    const auto considerVehicle = [&](const Vector3& position) {
        // マス間の移動中も、矢印と車両の幅が重なる範囲では高く保つ。
        if (std::abs(position.x - worldX) < gridSize_ &&
            std::abs(position.z - worldZ) < gridSize_) {
            const float jumpOffset = std::max(
                0.0f, position.y - BlockModelLayout::GetRailTopHeight(gridSize_));
            heightOffset = std::max(heightOffset, gridSize_ + jumpOffset);
        }
    };
    considerVehicle(GetWorldPosition());
    for (const auto* carriage : carriageTransforms_) {
        considerVehicle(carriage->Get().translate);
    }
    return heightOffset;
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

    transform_->Get().translate.y =
        BlockModelLayout::GetRailTopHeight(gridSize_) + GetRockThrowJumpOffset();
    SyncCarriageTransforms();
}

void GameComponents::TrainMovementComponent::AddCarriage(TransformComponent* carriageTransform) {
    if (!carriageTransform || traveledCells_.size() < carriageTransforms_.size() + 2) {
        return;
    }
    carriageTransforms_.push_back(carriageTransform);
    SyncCarriageTransforms();
}

void GameComponents::TrainMovementComponent::ProcessCarriageArrival(bool stationActivated) {
    ++traveledBlockCount_;
    traveledCells_.emplace_back(gridX_, gridZ_);
    // 先頭と同じ進捗で各車両もマス中央に到着する。経路上の到着マスで回復を判定する。
    for (std::size_t index = 0; index < carriageTransforms_.size(); ++index) {
        const std::size_t offset = index + 1;
        if (traveledCells_.size() <= offset) {
            break;
        }
        const auto& [carriageX, carriageZ] = traveledCells_[traveledCells_.size() - 1 - offset];
        hunger_->OnMonkeyEnteredCell(offset, carriageX, carriageZ);
    }
    if (stationActivated) {
        pendingStationSteps_.push_back(traveledBlockCount_);
    }

    // 最後尾は先頭から後続車両数だけ遅れている。駅の次の中央へ着くまで待つ。
    // 連続する駅でも、先の駅で増えた車両を含めた最後尾で毎回判定する。
    while (!pendingStationSteps_.empty() &&
        traveledBlockCount_ - pendingStationSteps_.front() >= carriageTransforms_.size() + 1) {
        pendingStationSteps_.pop_front();
        hunger_->AddMonkey();
    }

    // 次の連結用に最後尾の1マス後ろまで残し、走行距離に比例して履歴を増やさない。
    while (traveledCells_.size() > carriageTransforms_.size() + 2) {
        traveledCells_.pop_front();
    }
    SyncCarriageTransforms();
}

void GameComponents::TrainMovementComponent::SyncCarriageTransforms() {
    const float modelScale = BlockModelLayout::GetScale(gridSize_);
    for (std::size_t index = 0; index < carriageTransforms_.size(); ++index) {
        const std::size_t offset = index + 1;
        if (traveledCells_.size() <= offset) {
            break;
        }
        const std::size_t cellIndex = traveledCells_.size() - 1 - offset;
        const auto& [startX, startZ] = traveledCells_[cellIndex];
        const auto& [endX, endZ] = traveledCells_[cellIndex + 1];
        auto& carriage = carriageTransforms_[index]->Get();
        carriage.scale = { modelScale, modelScale, modelScale };
        carriage.translate = {
            (static_cast<float>(startX) + static_cast<float>(endX - startX) * movementProgress_) * gridSize_,
            BlockModelLayout::GetRailTopHeight(gridSize_),
            (static_cast<float>(startZ) + static_cast<float>(endZ - startZ) * movementProgress_) * gridSize_
        };
        carriage.rotate.y = std::atan2(
            static_cast<float>(endX - startX), static_cast<float>(endZ - startZ));
    }
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

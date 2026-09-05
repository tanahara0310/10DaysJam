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

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::RailBuilderComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "initialGridX", initialGridPosX_ },
        { "initialGridZ", initialGridPosZ_ },
        { "horizontalPrioritize", HorizontalPrioritize },
        { "undoHoldTime", undoPushMaxTime_ },
        { "undoInterval", undoInterval_ },
        { "buildHoldTime", buildPushMaxTime_ },
        { "buildInterval", buildInterval_ },
        { "height", height_ },
        { "pulseBaseScale", pulseBaseScale_ },
        { "pulseAmplitude", pulseAmplitude_ },
        { "pulseSpeed", pulseSpeed_ },
        { "rotationSpeed", rotationSpeed_ },
        { "groundCost", groundCost_ },
        { "waterCost", waterCost_ },
        { "stationReward", stationReward_ },
        { "resourceReward", resourceReward_ },
        { "maxSpeedRewardRatio", maxSpeedRewardRatio_ }
    };
}

void GameComponents::RailBuilderComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    initialGridPosX_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridX", initialGridPosX_));
    initialGridPosZ_ = std::max(0, JsonManager::SafeGet<int32_t>(j, "initialGridZ", initialGridPosZ_));
    HorizontalPrioritize = JsonManager::SafeGet<bool>(j, "horizontalPrioritize", HorizontalPrioritize);
    undoPushMaxTime_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "undoHoldTime", undoPushMaxTime_));
    undoInterval_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "undoInterval", undoInterval_));
    buildPushMaxTime_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "buildHoldTime", buildPushMaxTime_));
    buildInterval_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "buildInterval", buildInterval_));
    height_ = JsonManager::SafeGet<float>(j, "height", height_);
    pulseBaseScale_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "pulseBaseScale", pulseBaseScale_));
    pulseAmplitude_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "pulseAmplitude", pulseAmplitude_));
    pulseSpeed_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "pulseSpeed", pulseSpeed_));
    rotationSpeed_ = JsonManager::SafeGet<float>(j, "rotationSpeed", rotationSpeed_);
    groundCost_ = JsonManager::SafeGet<uint32_t>(j, "groundCost", groundCost_);
    waterCost_ = JsonManager::SafeGet<uint32_t>(j, "waterCost", waterCost_);
    stationReward_ = JsonManager::SafeGet<uint32_t>(j, "stationReward", stationReward_);
    resourceReward_ = JsonManager::SafeGet<uint32_t>(j, "resourceReward", resourceReward_);
    maxSpeedRewardRatio_ = std::max(1.0f,
        JsonManager::SafeGet<float>(j, "maxSpeedRewardRatio", maxSpeedRewardRatio_));
    gridPosX_ = initialGridPosX_;
    gridPosZ_ = initialGridPosZ_;
}

#ifdef USE_IMGUI
bool GameComponents::RailBuilderComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    changed |= ImGui::DragInt("初期X", &initialGridPosX_, 1.0f, 0, 500);
    changed |= ImGui::DragInt("初期Z", &initialGridPosZ_, 1.0f, 0, 100);
    changed |= ImGui::Checkbox("水平方向を優先", &HorizontalPrioritize);
    changed |= ImGui::DragFloat("Undo長押し時間", &undoPushMaxTime_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("Undo連続間隔", &undoInterval_, 0.01f, 0.01f, 2.0f);
    changed |= ImGui::DragFloat("設置長押し時間", &buildPushMaxTime_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("設置連続間隔", &buildInterval_, 0.01f, 0.01f, 2.0f);
    changed |= ImGui::DragFloat("表示高さ", &height_, 0.05f, -20.0f, 20.0f);
    changed |= ImGui::DragFloat("脈動基準スケール", &pulseBaseScale_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("脈動振幅", &pulseAmplitude_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("脈動速度", &pulseSpeed_, 0.05f, 0.0f, 30.0f);
    changed |= ImGui::DragFloat("回転速度", &rotationSpeed_, 0.05f, -30.0f, 30.0f);
    int groundCost = static_cast<int>(groundCost_);
    int waterCost = static_cast<int>(waterCost_);
    int stationReward = static_cast<int>(stationReward_);
    int resourceReward = static_cast<int>(resourceReward_);
    if (ImGui::DragInt("地上レールコスト", &groundCost, 1.0f, 0, 100)) { groundCost_ = static_cast<uint32_t>(groundCost); changed = true; }
    if (ImGui::DragInt("水上レールコスト", &waterCost, 1.0f, 0, 100)) { waterCost_ = static_cast<uint32_t>(waterCost); changed = true; }
    if (ImGui::DragInt("駅報酬", &stationReward, 1.0f, 0, 999)) { stationReward_ = static_cast<uint32_t>(stationReward); changed = true; }
    if (ImGui::DragInt("資源報酬", &resourceReward, 1.0f, 0, 999)) { resourceReward_ = static_cast<uint32_t>(resourceReward); changed = true; }
    changed |= ImGui::DragFloat("速度報酬の最大倍率", &maxSpeedRewardRatio_, 0.05f, 1.0f, 10.0f);
    return changed;
}
#endif

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
    transform_->Get().scale = {
        1.0f, pulseBaseScale_ + (sinf(timer_ * pulseSpeed_) * pulseAmplitude_), 1.0f };
    transform_->Get().rotate.y = timer_ * rotationSpeed_;

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

    // 移動入力は押した瞬間に1回処理し、長押し時はUndoと同じように
    // 一定時間経過後、一定間隔で繰り返す。
    bool isContinuousBuild = false;
    const bool isMovePressed =
        input.IsActionPressed(InputAction::MoveRight) ||
        input.IsActionPressed(InputAction::MoveLeft) ||
        input.IsActionPressed(InputAction::MoveForward) ||
        input.IsActionPressed(InputAction::MoveBack);
    if (isMovePressed) {
        buildPushTimer_ += Time::DeltaTime();
        if (buildPushTimer_ >= buildPushMaxTime_) {
            if (buildIntervalTimer_ <= 0.0f) {
                isContinuousBuild = true;
                buildIntervalTimer_ = buildInterval_;
            } else {
                buildIntervalTimer_ -= Time::DeltaTime();
            }
        }
    } else {
        buildPushTimer_ = 0.0f;
        buildIntervalTimer_ = 0.0f;
    }

    const bool isMoveTriggered =
        input.IsActionTriggered(InputAction::MoveRight) ||
        input.IsActionTriggered(InputAction::MoveLeft) ||
        input.IsActionTriggered(InputAction::MoveForward) ||
        input.IsActionTriggered(InputAction::MoveBack);
    if (!isMoveTriggered && !isContinuousBuild) {
        return;
    }

    const bool usePressedDirection = !isMoveTriggered;

    // X方向の移動量を計算する（右キー - 左キー）
    float moveX =
        static_cast<float>(usePressedDirection
            ? input.IsActionPressed(InputAction::MoveRight)
            : input.IsActionTriggered(InputAction::MoveRight)) -
        static_cast<float>(usePressedDirection
            ? input.IsActionPressed(InputAction::MoveLeft)
            : input.IsActionTriggered(InputAction::MoveLeft));

    // Z方向の移動量を計算する（前キー - 後キー）
    float moveZ =
        static_cast<float>(usePressedDirection
            ? input.IsActionPressed(InputAction::MoveForward)
            : input.IsActionTriggered(InputAction::MoveForward)) -
        static_cast<float>(usePressedDirection
            ? input.IsActionPressed(InputAction::MoveBack)
            : input.IsActionTriggered(InputAction::MoveBack));

    // 反対方向の入力が同時に発生した場合は移動しない。
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

    // 最後に設置したレールの一つ前へ戻ろうとした場合は、
    // 既設レールへの移動ではなく最後のレールのUndoとして扱う。
    auto& railMap = railPath_->GetRailMap();
    const auto& railUndoStack = railPath_->GetRailUndoStack();
    if (!railUndoStack.empty()) {
        const std::pair<int32_t, int32_t> previousRail = railUndoStack.size() >= 2
            ? railUndoStack[railUndoStack.size() - 2]
            : railMap.back();
        if (nextX == previousRail.first && nextZ == previousRail.second) {
            TryUndoLastRail();
            return;
        }
    }

    // 既にレールがあるマスへは移動しない
    for (auto& rail : railMap) {
        if (rail.first == nextX && rail.second == nextZ) {
            Logger::GetInstance().Infof(
                LogCategory::Game,
                "RailBuilder: 既設レールのため移動を中止しました ({}, {})",
                nextX, nextZ);
            return;
        }
    }
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

    // Voidとバナナの木は非建設マスとして扱う。
    if (mapChip == MapChipType::Void || mapChip == MapChipType::BananaTree) {
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailBuilder: 建設不可チップのためレールを設置できません ({}, {})",
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
        resourceCost = groundCost_;
    } else if (mapChip == MapChipType::Water) {
        resourceCost = waterCost_;
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
        const uint32_t reward = CalculateSpeedReward(stationReward_);
        resourceManager_->AddResource(reward);
        railPath_->ConfirmAllPendingRailPlacements();
        Logger::GetInstance().Infof(
            LogCategory::Game,
            "RailBuilder: 駅に到達しました (報酬={}, 駅までのレールを確定)",
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
    return baseAmount;
}

void GameComponents::RailBuilderComponent::SyncTransformToGrid() {
    if (!transform_) {
        return;
    }

    transform_->Get().translate.x = static_cast<float>(gridPosX_) * gridSize_;
    transform_->Get().translate.z = static_cast<float>(gridPosZ_) * gridSize_;

    transform_->Get().translate.y = height_;
}

void GameComponents::RailBuilderComponent::SetGridSize(float size) {
    gridSize_ = size;  
}

void GameComponents::RailBuilderComponent::SetHorizontalPrioritize(bool prioritize) {
    HorizontalPrioritize = prioritize;
}   

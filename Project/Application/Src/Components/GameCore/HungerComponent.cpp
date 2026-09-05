#include "pch.h"
#include "HungerComponent.h"

#include "Components/Building/MapChipData.h"
#include "Components/Building/MapGeneratorComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <array>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::HungerComponent::OnSerialize() const
{
    return {
        { "initialHunger", initialHunger_ },
        { "maximumHunger", maximumHunger_ },
        { "drainPerSecond", drainPerSecond_ },
        { "bananaRecovery", bananaRecovery_ }
    };
}

void GameComponents::HungerComponent::OnDeserialize(const json& j)
{
    maximumHunger_ = std::max(
        0.01f, JsonManager::SafeGet<float>(j, "maximumHunger", maximumHunger_));
    initialHunger_ = std::clamp(
        JsonManager::SafeGet<float>(j, "initialHunger", initialHunger_),
        0.0f, maximumHunger_);
    drainPerSecond_ = std::max(
        0.0f, JsonManager::SafeGet<float>(j, "drainPerSecond", drainPerSecond_));
    bananaRecovery_ = std::max(
        0.0f, JsonManager::SafeGet<float>(j, "bananaRecovery", bananaRecovery_));
    currentHunger_ = initialHunger_;
}

#ifdef USE_IMGUI
bool GameComponents::HungerComponent::DrawInspector()
{
    bool changed = false;
    if (ImGui::DragFloat("最大空腹値", &maximumHunger_, 1.0f, 0.01f, 10000.0f)) {
        maximumHunger_ = std::max(maximumHunger_, 0.01f);
        initialHunger_ = std::min(initialHunger_, maximumHunger_);
        currentHunger_ = std::min(currentHunger_, maximumHunger_);
        changed = true;
    }
    if (ImGui::DragFloat("初期空腹値", &initialHunger_, 1.0f, 0.0f, maximumHunger_)) {
        initialHunger_ = std::clamp(initialHunger_, 0.0f, maximumHunger_);
        currentHunger_ = initialHunger_;
        changed = true;
    }
    if (ImGui::DragFloat("1秒あたりの減少量", &drainPerSecond_, 0.1f, 0.0f, 1000.0f)) {
        drainPerSecond_ = std::max(drainPerSecond_, 0.0f);
        changed = true;
    }
    if (ImGui::DragFloat("バナナ回復量", &bananaRecovery_, 1.0f, 0.0f, 10000.0f)) {
        bananaRecovery_ = std::max(bananaRecovery_, 0.0f);
        changed = true;
    }
    ImGui::Separator();
    ImGui::Text("現在値: %.1f / %.1f", currentHunger_, maximumHunger_);
    ImGui::TextDisabled("減少中: %s", isDraining_ ? "はい" : "いいえ");
    ImGui::TextDisabled("発動済み方向数: %zu", activatedBananaSides_.size());
    return changed;
}
#endif

void GameComponents::HungerComponent::Start()
{
    currentHunger_ = std::clamp(initialHunger_, 0.0f, maximumHunger_);
    isDraining_ = false;
    gameOverRequested_ = false;
    activatedBananaSides_.clear();

    if (!mapGenerator_ || !gameManager_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "HungerComponent: MapGenerator または GameManager が未設定です");
        SetEnabled(false);
    }
}

void GameComponents::HungerComponent::Update()
{
    if (!isDraining_ || gameOverRequested_ || !gameManager_ ||
        gameManager_->GetPhase() != GameManagerComponent::Phase::Playing) {
        return;
    }

    const float deltaTime = std::max(0.0f, Time::DeltaTime());
    currentHunger_ = std::max(0.0f, currentHunger_ - drainPerSecond_ * deltaTime);
    if (currentHunger_ <= 0.0f) {
        gameOverRequested_ = true;
        gameManager_->RequestGameOver();
        Logger::GetInstance().Infof(LogCategory::Game, "空腹値が0になりました");
    }
}

void GameComponents::HungerComponent::StartDraining()
{
    if (!isDraining_ && !gameOverRequested_) {
        isDraining_ = true;
        Logger::GetInstance().Infof(LogCategory::Game, "空腹値の減少を開始しました");
    }
}

void GameComponents::HungerComponent::OnTrainEnteredCell(int32_t gridX, int32_t gridZ)
{
    if (!mapGenerator_ || !gameManager_ ||
        gameManager_->GetPhase() != GameManagerComponent::Phase::Playing) {
        return;
    }

    constexpr std::array<std::pair<int32_t, int32_t>, 4> kDirections = {
        std::pair{ 1, 0 }, std::pair{ -1, 0 },
        std::pair{ 0, 1 }, std::pair{ 0, -1 }
    };

    std::size_t triggeredCount = 0;
    for (const auto& [offsetX, offsetZ] : kDirections) {
        const int32_t treeX = gridX + offsetX;
        const int32_t treeZ = gridZ + offsetZ;
        if (treeX < 0 || treeZ < 0) {
            continue;
        }
        if (mapGenerator_->GetMapChip(
                static_cast<std::size_t>(treeX),
                static_cast<std::size_t>(treeZ)) != MapChipType::BananaTree) {
            continue;
        }

        if (activatedBananaSides_.emplace(treeX, treeZ, gridX, gridZ).second) {
            ++triggeredCount;
        }
    }

    if (triggeredCount == 0) {
        return;
    }

    const float recovery = bananaRecovery_ * static_cast<float>(triggeredCount);
    currentHunger_ = std::min(maximumHunger_, currentHunger_ + recovery);
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "バナナの木が {} 本発動しました (回復量={}, 現在値={})",
        triggeredCount, recovery, currentHunger_);
}

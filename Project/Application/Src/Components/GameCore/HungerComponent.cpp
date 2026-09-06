#include "pch.h"
#include "HungerComponent.h"

#include "Components/Building/MapChipData.h"
#include "Components/Building/MapGeneratorComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::HungerComponent::OnSerialize() const
{
    return {
        { "initialStamina", initialHunger_ },
        { "maximumStamina", maximumHunger_ },
        { "bananaRecovery", bananaRecovery_ },
        { "additionalMonkeyCostRate", additionalMonkeyCostRate_ }
    };
}

void GameComponents::HungerComponent::OnDeserialize(const json& j)
{
    maximumHunger_ = std::max(0.01f,
        JsonManager::SafeGet<float>(j, "maximumStamina",
            JsonManager::SafeGet<float>(j, "maximumHunger", maximumHunger_)));
    initialHunger_ = std::clamp(
        JsonManager::SafeGet<float>(j, "initialStamina",
            JsonManager::SafeGet<float>(j, "initialHunger", initialHunger_)),
        0.0f, maximumHunger_);
    bananaRecovery_ = std::max(
        0.0f, JsonManager::SafeGet<float>(j, "bananaRecovery", bananaRecovery_));
    additionalMonkeyCostRate_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "additionalMonkeyCostRate", additionalMonkeyCostRate_));
    currentHunger_ = initialHunger_;
}

#ifdef USE_IMGUI
bool GameComponents::HungerComponent::DrawInspector()
{
    bool changed = false;
    if (ImGui::DragFloat("最大スタミナ", &maximumHunger_, 1.0f, 0.01f, 10000.0f)) {
        maximumHunger_ = std::max(maximumHunger_, 0.01f);
        initialHunger_ = std::min(initialHunger_, maximumHunger_);
        currentHunger_ = std::min(currentHunger_, maximumHunger_);
        changed = true;
    }
    if (ImGui::DragFloat("初期スタミナ", &initialHunger_, 1.0f, 0.0f, maximumHunger_)) {
        initialHunger_ = std::clamp(initialHunger_, 0.0f, maximumHunger_);
        currentHunger_ = initialHunger_;
        changed = true;
    }
    if (ImGui::DragFloat("バナナ回復量", &bananaRecovery_, 1.0f, 0.0f, 10000.0f)) {
        bananaRecovery_ = std::max(bananaRecovery_, 0.0f);
        changed = true;
    }
    changed |= ImGui::DragFloat(
        "サル1匹追加ごとの消費倍率", &additionalMonkeyCostRate_, 0.01f, 0.0f, 10.0f);
    ImGui::Separator();
    ImGui::Text("現在値: %.1f / %.1f", currentHunger_, maximumHunger_);
    ImGui::TextDisabled("サル数: %zu / 消費倍率: %.2f", monkeyCount_, GetCostMultiplier());
    ImGui::TextDisabled("発動済み方向数: %zu", activatedBananaSides_.size());
    return changed;
}
#endif

void GameComponents::HungerComponent::Start()
{
    currentHunger_ = std::clamp(initialHunger_, 0.0f, maximumHunger_);
    gameOverRequested_ = false;
    monkeyCount_ = 1;
    activatedBananaSides_.clear();
    activatedStations_.clear();

    if (!mapGenerator_ || !gameManager_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "HungerComponent: MapGenerator または GameManager が未設定です");
        SetEnabled(false);
    }
}

void GameComponents::HungerComponent::Update()
{
    // スタミナは行動時だけ消費する。時間経過では減らさない。
}

bool GameComponents::HungerComponent::OnTrainEnteredCell(int32_t gridX, int32_t gridZ)
{
    if (!mapGenerator_ || !gameManager_ ||
        gameManager_->GetPhase() != GameManagerComponent::Phase::Playing) {
        return false;
    }

    bool stationActivated = false;
    if (gridX >= 0 && gridZ >= 0 &&
        mapGenerator_->GetMapChip(static_cast<std::size_t>(gridX),
            static_cast<std::size_t>(gridZ)) == MapChipType::Station &&
        activatedStations_.emplace(gridX, gridZ).second) {
        stationActivated = true;
        ++monkeyCount_;
        if (onMonkeyAdded_) {
            onMonkeyAdded_(monkeyCount_);
        }
        Logger::GetInstance().Infof(
            LogCategory::Game, "駅到着: サルが増えました ({}匹)", monkeyCount_);
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
        return stationActivated;
    }

    const float recovery = bananaRecovery_ * static_cast<float>(triggeredCount);
    currentHunger_ = std::min(maximumHunger_, currentHunger_ + recovery);
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "バナナの木が {} 本発動しました (回復量={}, 現在値={})",
        triggeredCount, recovery, currentHunger_);
    return stationActivated;
}

float GameComponents::HungerComponent::GetCostMultiplier() const
{
    return 1.0f + static_cast<float>(monkeyCount_ - 1) * additionalMonkeyCostRate_;
}

float GameComponents::HungerComponent::CalculateActionCost(float baseAmount) const
{
    return std::ceil(std::max(0.0f, baseAmount) * GetCostMultiplier());
}

bool GameComponents::HungerComponent::TryConsumeStamina(float amount)
{
    if (amount <= 0.0f) {
        return true;
    }
    if (gameOverRequested_ || !gameManager_ ||
        gameManager_->GetPhase() != GameManagerComponent::Phase::Playing) {
        return false;
    }
    if (currentHunger_ < amount) {
        return false;
    }

    currentHunger_ = std::max(0.0f, currentHunger_ - amount);
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "スタミナを消費しました (消費量={}, 現在値={})",
        amount, currentHunger_);

    if (currentHunger_ <= 0.0f) {
        gameOverRequested_ = true;
        gameManager_->RequestGameOver();
    }
    return true;
}

void GameComponents::HungerComponent::AddStamina(float amount)
{
    if (amount > 0.0f) {
        currentHunger_ = std::min(maximumHunger_, currentHunger_ + amount);
    }
}

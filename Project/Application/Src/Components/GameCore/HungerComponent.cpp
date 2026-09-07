#include "pch.h"
#include "HungerComponent.h"

#include "Components/Building/MapChipData.h"
#include "Components/Building/MapGeneratorComponent.h"
#include "Components/GameCore/GameManagerComponent.h"
#include "Components/GameCore/GameSettingsComponent.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#include "Editor/ImGui/CVarPanel.h"
#endif

using namespace CoreEngine;

#ifdef USE_IMGUI
bool GameComponents::HungerComponent::DrawInspector()
{
    const bool changed = CVarUI::DrawTree("Game.Stamina");
    UI::Hint("変更はCVars.jsonへ自動保存されます。初期スタミナはシーン再読み込み時に反映されます。");
    ImGui::Separator();
    ImGui::Text("共通スタミナ: %.1f / %.1f", GetCurrentHunger(), GetMaximumHunger());
    ImGui::TextDisabled("サル数: %zu / 消費倍率: %.2f", monkeyCount_, GetCostMultiplier());
    ImGui::TextDisabled("バナナ回復の発動数: %zu", activatedBananaSides_.size());
    return changed;
}
#endif

void GameComponents::HungerComponent::Start()
{
    currentHunger_ = std::clamp(GameSettings::InitialStamina.Get(), 0.0f, GetMaximumHunger());
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
    // 上限をインスペクターで下げた場合は、実行中の共通スタミナも制限する。
    currentHunger_ = GetCurrentHunger();
}

bool GameComponents::HungerComponent::OnTrainEnteredCell(int32_t gridX, int32_t gridZ)
{
    if (!mapGenerator_ || !gameManager_ ||
        gameManager_->GetPhase() != GameManagerComponent::Phase::Playing) {
        return false;
    }

    bool stationActivated = false;
    if (gridX >= 0 && gridZ >= 0 &&
        mapGenerator_->IsStationRailCell(static_cast<std::size_t>(gridX),
            static_cast<std::size_t>(gridZ)) &&
        activatedStations_.emplace(gridX, gridZ + 1).second) {
        stationActivated = true;
    }

    OnMonkeyEnteredCell(0, gridX, gridZ);
    return stationActivated;
}

void GameComponents::HungerComponent::OnMonkeyEnteredCell(
    std::size_t monkeyIndex, int32_t gridX, int32_t gridZ)
{
    if (monkeyIndex >= monkeyCount_ || !mapGenerator_ || !gameManager_ ||
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

        if (activatedBananaSides_.emplace(monkeyIndex, treeX, treeZ, gridX, gridZ).second) {
            ++triggeredCount;
        }
    }

    if (triggeredCount == 0) {
        return;
    }

    const float recovery = std::max(0.0f, GameSettings::BananaRecovery.Get()) *
        static_cast<float>(triggeredCount);
    AddStamina(recovery);
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "サル {} がバナナの木を {} 本通過しました (回復量={}, 共通スタミナ={})",
        monkeyIndex + 1, triggeredCount, recovery, currentHunger_);
}

void GameComponents::HungerComponent::AddMonkey()
{
    ++monkeyCount_;
    if (onMonkeyAdded_) {
        onMonkeyAdded_(monkeyCount_);
    }
    Logger::GetInstance().Infof(
        LogCategory::Game, "駅からトロッコを連結: サルが増えました ({}匹)", monkeyCount_);
}

float GameComponents::HungerComponent::GetCostMultiplier() const
{
    return 1.0f + static_cast<float>(monkeyCount_ - 1) *
        std::max(0.0f, GameSettings::AdditionalMonkeyCostRate.Get());
}

float GameComponents::HungerComponent::GetCurrentHunger() const
{
    return std::min(currentHunger_, GetMaximumHunger());
}

float GameComponents::HungerComponent::GetMaximumHunger() const
{
    return std::max(0.01f, GameSettings::MaximumStamina.Get());
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
    if (GetCurrentHunger() < amount) {
        return false;
    }

    currentHunger_ = std::max(0.0f, GetCurrentHunger() - amount);
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "スタミナを消費しました (消費量={}, 現在値={})",
        amount, currentHunger_);

    if (currentHunger_ <= 0.0f && GameSettings::GameOverAtZeroStamina.Get()) {
        gameOverRequested_ = true;
        gameManager_->RequestGameOver();
    }
    return true;
}

void GameComponents::HungerComponent::AddStamina(float amount)
{
    if (amount > 0.0f) {
        currentHunger_ = std::min(GetMaximumHunger(), GetCurrentHunger() + amount);
    }
}

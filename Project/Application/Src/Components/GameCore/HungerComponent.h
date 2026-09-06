#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <tuple>

namespace GameComponents
{
    class GameManagerComponent;
    class MapGeneratorComponent;

    /// @brief 建設に使うスタミナ、バナナ回復、サルによる消費倍率を管理する。
    class HungerComponent final : public CoreEngine::IComponent
    {
    public:
        explicit HungerComponent(
            MapGeneratorComponent* mapGenerator = nullptr,
            GameManagerComponent* gameManager = nullptr)
            : mapGenerator_(mapGenerator), gameManager_(gameManager) {}

        const char* GetTypeName() const override { return "Hunger"; }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "スタミナ"; }
        bool DrawInspector() override;
#endif

        void Start() override;
        void Update() override;

        /// @brief 列車到着時の駅・バナナ処理。未訪問の駅ならtrueを返す。
        bool OnTrainEnteredCell(int32_t gridX, int32_t gridZ);
        /// @brief サル倍率込みの行動コストを求める。
        float CalculateActionCost(float baseAmount) const;
        /// @brief スタミナが足りる場合だけ消費する。
        bool TryConsumeStamina(float amount);
        /// @brief Undoなどでスタミナを回復する。
        void AddStamina(float amount);
        void SetMonkeyAddedCallback(std::function<void(std::size_t)> callback)
        {
            onMonkeyAdded_ = std::move(callback);
        }

        float GetCurrentHunger() const { return currentHunger_; }
        float GetMaximumHunger() const { return maximumHunger_; }
        std::size_t GetMonkeyCount() const { return monkeyCount_; }
        float GetCostMultiplier() const;

    private:
        using BananaTriggerKey = std::tuple<int32_t, int32_t, int32_t, int32_t>;

        MapGeneratorComponent* mapGenerator_ = nullptr;
        GameManagerComponent* gameManager_ = nullptr;

        float initialHunger_ = 100.0f;
        float maximumHunger_ = 100.0f;
        float bananaRecovery_ = 20.0f;
        float additionalMonkeyCostRate_ = 0.25f;
        float currentHunger_ = 100.0f;
        bool gameOverRequested_ = false;
        std::size_t monkeyCount_ = 1;
        std::function<void(std::size_t)> onMonkeyAdded_;

        // 木の座標と、列車が通った隣接マスの組ごとに一度だけ発動させる。
        std::set<BananaTriggerKey> activatedBananaSides_;
        std::set<std::pair<int32_t, int32_t>> activatedStations_;
    };
}

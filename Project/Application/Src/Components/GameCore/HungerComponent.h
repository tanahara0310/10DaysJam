#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <set>
#include <tuple>
#include <utility>

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

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "スタミナ"; }
        bool DrawInspector() override;
#endif

        void Start() override;
        void Update() override;

        /// @brief 先頭車両到着時の駅前レール・バナナ処理。未訪問の駅前ならtrueを返す。
        bool OnTrainEnteredCell(int32_t gridX, int32_t gridZ);
        /// @brief 各サルの通過時に共通スタミナを回復する。先頭のサルはindex=0。
        void OnMonkeyEnteredCell(std::size_t monkeyIndex, int32_t gridX, int32_t gridZ);
        /// @brief 駅から新しいトロッコが連結されるタイミングでサルを1匹追加する。
        void AddMonkey();
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
        /// @brief サルを送り出した駅チップの位置を、サルが増えるたびに通知する。
        void SetStationPopCallback(std::function<void(int32_t, int32_t)> callback)
        {
            onStationPop_ = std::move(callback);
        }

        float GetCurrentHunger() const;
        float GetMaximumHunger() const;
        std::size_t GetMonkeyCount() const { return monkeyCount_; }
        float GetCostMultiplier() const;

    private:
        using BananaTriggerKey = std::tuple<std::size_t, int32_t, int32_t, int32_t, int32_t>;

        MapGeneratorComponent* mapGenerator_ = nullptr;
        GameManagerComponent* gameManager_ = nullptr;

        float currentHunger_ = 100.0f; // すべてのサルで共有するスタミナ。
        bool gameOverRequested_ = false;
        std::size_t monkeyCount_ = 1;
        std::function<void(std::size_t)> onMonkeyAdded_;
        std::function<void(int32_t, int32_t)> onStationPop_;
        // 発動した駅チップの位置。サルが増えるたびに古い順へ取り出す
        std::deque<std::pair<int32_t, int32_t>> pendingMonkeyStations_;

        // サルごとに、木の座標と通った隣接マスの組につき一度だけ発動させる。
        std::set<BananaTriggerKey> activatedBananaSides_;
        std::set<std::pair<int32_t, int32_t>> activatedStations_;
    };
}

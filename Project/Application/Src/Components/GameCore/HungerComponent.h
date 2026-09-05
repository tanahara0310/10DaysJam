#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <tuple>

namespace GameComponents
{
    class GameManagerComponent;
    class MapGeneratorComponent;

    /// @brief 列車の空腹値とバナナの木による回復を管理する。
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
        const char* GetInspectorName() const override { return "空腹値"; }
        bool DrawInspector() override;
#endif

        void Start() override;
        void Update() override;

        /// @brief 列車が初めて動き始めたときに空腹値の減少を開始する。
        void StartDraining();
        /// @brief 列車が新しいマスへ到着したとき、隣接するバナナの木を判定する。
        void OnTrainEnteredCell(int32_t gridX, int32_t gridZ);

        float GetCurrentHunger() const { return currentHunger_; }
        float GetMaximumHunger() const { return maximumHunger_; }
        bool IsDraining() const { return isDraining_; }

    private:
        using BananaTriggerKey = std::tuple<int32_t, int32_t, int32_t, int32_t>;

        MapGeneratorComponent* mapGenerator_ = nullptr;
        GameManagerComponent* gameManager_ = nullptr;

        float initialHunger_ = 100.0f;
        float maximumHunger_ = 100.0f;
        float drainPerSecond_ = 1.0f;
        float bananaRecovery_ = 20.0f;
        float currentHunger_ = 100.0f;
        bool isDraining_ = false;
        bool gameOverRequested_ = false;

        // 木の座標と、列車が通った隣接マスの組ごとに一度だけ発動させる。
        std::set<BananaTriggerKey> activatedBananaSides_;
    };
}

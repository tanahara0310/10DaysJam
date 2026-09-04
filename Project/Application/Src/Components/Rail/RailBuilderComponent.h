#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstdint>
#include <functional>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    class RailPathComponent;
    class RailResourceManagerComponent;
    class MapGeneratorComponent;
    class TrainMovementComponent;
}   

namespace GameComponents
{
    // 入力に応じてグリッド単位で移動するコンポーネント
    class RailBuilderComponent final
        : public CoreEngine::IComponent {
    public:
        explicit RailBuilderComponent(
            float gridSize = 5.0f, int32_t gridPosX = 0,int32_t gridPosZ = 0,
            GameComponents::RailPathComponent* railPath = nullptr,
            GameComponents::RailResourceManagerComponent* resourceManager = nullptr,
            GameComponents::MapGeneratorComponent* mapGenerator = nullptr,
            GameComponents::TrainMovementComponent* trainMovement = nullptr,
            std::function<void()> OnBuildSE = nullptr,
            std::function<void()> OnUndoSE = nullptr)
            : gridSize_(gridSize), initialGridPosX_(gridPosX), initialGridPosZ_(gridPosZ),
              gridPosX_(gridPosX), gridPosZ_(gridPosZ),
              railPath_(railPath), resourceManager_(resourceManager),
              mapGenerator_(mapGenerator), trainMovement_(trainMovement),
              OnBuildSE_(OnBuildSE), OnUndoSE_(OnUndoSE) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "RailBuilder";
        }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "レールビルダー"; }
        bool DrawInspector() override;
#endif

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // グリッドサイズを設定する
        void SetGridSize(float size);
        // 水平方向優先かどうかを設定する
        void SetHorizontalPrioritize(bool prioritize);

    private:
        // 論理グリッド座標を Transform のワールド座標へ反映する
        void SyncTransformToGrid();
        // 最後に置いたレールを撤去して、消費したレールを回収する
        bool TryUndoLastRail();
        // 列車の現在速度に応じた整数の報酬量を求める
        uint32_t CalculateSpeedReward(uint32_t baseAmount) const;

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;
        GameComponents::RailResourceManagerComponent* resourceManager_ = nullptr;
        GameComponents::MapGeneratorComponent* mapGenerator_ = nullptr;
        GameComponents::TrainMovementComponent* trainMovement_ = nullptr;

        // 左・後ろ方向へ移動したときの符号なし整数アンダーフローを避ける
        int32_t initialGridPosX_ = 0;
        int32_t initialGridPosZ_ = 0;
        int32_t gridPosX_ = 0;
        int32_t gridPosZ_ = 0;

        float gridSize_ = 5.0f;
        bool HorizontalPrioritize = true;

        float undoPushTimer_ = 0.0f;
        float undoPushMaxTime_ = 0.3f;
        float undoInterval_ = 0.05f;
        float undoIntervalTimer_ = 0.0f;

        float buildPushTimer_ = 0.0f;
        float buildPushMaxTime_ = 0.3f;
        float buildInterval_ = 0.05f;
        float buildIntervalTimer_ = 0.0f;

        float timer_ = 0.0f;
        float height_ = 1.0f;
        float pulseBaseScale_ = 0.8f;
        float pulseAmplitude_ = 0.2f;
        float pulseSpeed_ = 5.0f;
        float rotationSpeed_ = 2.0f;

        uint32_t groundCost_ = 1;
        uint32_t waterCost_ = 2;
        uint32_t stationReward_ = 15;
        uint32_t resourceReward_ = 5;
        float maxSpeedRewardRatio_ = 2.0f;

        std::function<void()> OnBuildSE_ = nullptr;
        std::function<void()> OnUndoSE_ = nullptr;
    };
}

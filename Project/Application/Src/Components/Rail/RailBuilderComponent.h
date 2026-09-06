#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstdint>
#include <deque>
#include <functional>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    class RailPathComponent;
    class MapGeneratorComponent;
    class TrainMovementComponent;
    class HungerComponent;
    class RockThrowComponent;
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
            GameComponents::MapGeneratorComponent* mapGenerator = nullptr,
            GameComponents::TrainMovementComponent* trainMovement = nullptr,
            GameComponents::HungerComponent* hunger = nullptr,
            GameComponents::RockThrowComponent* rockThrow = nullptr,
            std::function<void()> OnBuildSE = nullptr,
            std::function<void()> OnUndoSE = nullptr,
            std::function<void()> OnFailureSE = nullptr)
            : gridSize_(gridSize), initialGridPosX_(gridPosX), initialGridPosZ_(gridPosZ),
              gridPosX_(gridPosX), gridPosZ_(gridPosZ),
              railPath_(railPath),
              mapGenerator_(mapGenerator), trainMovement_(trainMovement),
              hunger_(hunger), rockThrow_(rockThrow),
              OnBuildSE_(OnBuildSE), OnUndoSE_(OnUndoSE), OnFailureSE_(OnFailureSE) {
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
        void SetInsufficientFeedback(std::function<void()> onStaminaInsufficient);

    private:
        // 論理グリッド座標を Transform のワールド座標へ反映する
        void SyncTransformToGrid();
        // 最後に置いたレールを撤去して、消費したレールを回収する
        bool TryUndoLastRail();
        // キュー先頭の岩へ投石を開始する
        void StartNextRockThrow();
        // 投石の着弾時に岩を地面へ変え、カーソルを通常位置へ戻す
        void CompleteRockBreak();
        void NotifyStaminaInsufficient();

        struct RockBreakRequest {
            int32_t gridX = 0;
            int32_t gridZ = 0;
        };

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;
        GameComponents::MapGeneratorComponent* mapGenerator_ = nullptr;
        GameComponents::TrainMovementComponent* trainMovement_ = nullptr;
        GameComponents::HungerComponent* hunger_ = nullptr;
        GameComponents::RockThrowComponent* rockThrow_ = nullptr;

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
        float rockCursorHeightOffset_ = 1.0f;
        float rockThrowStartHeight_ = 0.5f;
        float rockImpactHeight_ = 0.7f;
        float railStaminaCost_ = 2.0f;
        float rockStaminaCost_ = 20.0f;
        float bridgeStaminaCost_ = 5.0f;
        bool isBreakingRock_ = false;
        bool isCursorAboveRock_ = false;
        std::deque<RockBreakRequest> rockBreakQueue_;

        std::function<void()> OnBuildSE_ = nullptr;
        std::function<void()> OnUndoSE_ = nullptr;
        std::function<void()> OnFailureSE_ = nullptr;
        std::function<void()> OnStaminaInsufficient_ = nullptr;
    };
}

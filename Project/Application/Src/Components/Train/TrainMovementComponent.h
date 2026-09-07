#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents {
    class RailPathComponent;
    class GameManagerComponent;
    class HungerComponent;
}

namespace GameComponents
{
    // レールの上を移動するコンポーネント,レールパスが必要
    class TrainMovementComponent final
        : public CoreEngine::IComponent {
    public:
        explicit TrainMovementComponent(
            float gridSize = 5.0f, float moveSpeed = 0.5f,
            int32_t gridX = 0, int32_t gridZ = 0,
            GameComponents::RailPathComponent* railPath = nullptr,
            GameManagerComponent* gameManager = nullptr,
            HungerComponent* hunger = nullptr)
            : railPath_(railPath), gameManager_(gameManager), hunger_(hunger), gridSize_(gridSize),
              initialMoveSpeed_(moveSpeed), moveSpeed_(moveSpeed),
              initialGridX_(gridX), initialGridZ_(gridZ), gridX_(gridX), gridZ_(gridZ) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "TrainMovement";
        }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "列車移動"; }
        bool DrawInspector() override;
#endif

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // 発車後に進めるレールがなくなったか
        bool IsGameOver() const;

        float GetMoveSpeed() const { return moveSpeed_; }
        CoreEngine::Vector3 GetWorldPosition() const;
        // 指定位置に重なる先頭・後続車両の上へ矢印を置くための高さ補正。
        float GetCursorHeightOffsetAt(float worldX, float worldZ) const;
        uint32_t GetHorizontalProgressBlocks() const { return horizontalProgressBlocks_; }
        float GetMinMoveSpeed() const { return minMoveSpeed_; }
        float GetSpeedRatio() const {
            return initialMoveSpeed_ > 0.0f ? moveSpeed_ / initialMoveSpeed_ : 1.0f;
        }

        // グリッドサイズを設定する
        void SetGridSize(float size);
        // 最後尾に車両を連結する。車両は親を持たず、通過済みレールを1マス間隔で追従する。
        void AddCarriage(CoreEngine::TransformComponent* carriageTransform);
        // 岩破壊の投石中だけ列車の移動を停止・再開する
        void SetRockBreakPaused(bool paused) { isPausedForRockBreak_ = paused; }
        // 投石開始時に、その場でのジャンプを再生する
        void PlayRockThrowJump();

    private:
        // 終端検知を一か所に集め、終了処理は GameManager に委譲する。
        void NotifyGameOver();
        // 未確定レールを次の目的地として保存し、そのレールを確定する
        bool BeginNextSegment();
        // 現在の移動進捗を Transform に反映する
        void SyncTransformToProgress();
        void SyncCarriageTransforms();
        // マス中央への到着を記録し、最後尾が駅の次のマス中央へ着いたら連結する。
        void ProcessCarriageArrival(bool stationActivated);
        void UpdateRockThrowJump(float deltaTime);
        float GetRockThrowJumpOffset() const;
        // 移動方向に合わせて Y 軸回転を更新する
        void UpdateRotation();

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;
        GameManagerComponent* gameManager_ = nullptr;
        HungerComponent* hunger_ = nullptr;
        float gridSize_ = 5.0f;

        float initialMoveSpeed_ = 0.5f;
        float moveSpeed_ = 0.5f; // 移動速度（グリッド単位/秒）
        int32_t initialGridX_ = 0;
        int32_t initialGridZ_ = 0;
        int32_t gridX_ = 0; // 現在のグリッドX座標
        int32_t gridZ_ = 0; // 現在のグリッドZ座標
        int32_t destinationGridX_ = 0;
        int32_t destinationGridZ_ = 0;

        float movementProgress_ = 0.0f;
        std::vector<CoreEngine::TransformComponent*> carriageTransforms_;
        std::deque<std::pair<int32_t, int32_t>> traveledCells_;
        std::deque<std::size_t> pendingStationSteps_;
        std::size_t traveledBlockCount_ = 0;
        uint32_t horizontalProgressBlocks_ = 0;
        float rockThrowJumpHeight_ = 0.6f;
        float rockThrowJumpDuration_ = 0.35f;
        float rockThrowJumpElapsed_ = 0.0f;
        bool isMoving_ = false;
        bool isRockThrowJumping_ = false;
        bool hasStarted_ = false;
        bool isGameOver_ = false;
        bool isPausedForRockBreak_ = false;

        float minMoveSpeed_ = 0.5f; // 最低移動速度
        std::size_t speedIncreaseIntervalBlocks_ = 20;
        float speedIncreaseAmount_ = 0.25f;
        float maximumMoveSpeed_ = 8.0f;
        float stationSlowdownMultiplier_ = 0.5f;
        float stationSlowdownDuration_ = 2.0f;
        float stationSlowdownRemaining_ = 0.0f;
        std::size_t requiredRailCount_ = 5;
    };
}

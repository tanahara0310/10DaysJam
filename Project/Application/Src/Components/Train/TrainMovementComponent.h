#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

#include <cstddef>
#include <cstdint>

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
        float GetMinMoveSpeed() const { return minMoveSpeed_; }
        float GetSpeedRatio() const {
            return minMoveSpeed_ > 0.0f ? moveSpeed_ / minMoveSpeed_ : 1.0f;
        }

        // グリッドサイズを設定する
        void SetGridSize(float size);
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
        // 確定レールへ高速発進するときのジャンプを進める
        void UpdateBoostJump(float deltaTime);
        // 現在のジャンプによる高さ加算値を取得する
        float GetBoostJumpOffset() const;
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
        float completedRailPauseDuration_ = 1.0f;
        float completedRailPauseRemaining_ = 0.0f;
        float boostJumpHeight_ = 0.8f;
        float boostJumpDuration_ = 1.0f;
        float boostJumpElapsed_ = 0.0f;
        float rockThrowJumpHeight_ = 0.6f;
        float rockThrowJumpDuration_ = 0.35f;
        float rockThrowJumpElapsed_ = 0.0f;
        bool isMoving_ = false;
        bool isMovingOnCompletedRail_ = false;
        bool isBoostJumping_ = false;
        bool isRockThrowJumping_ = false;
        bool hasStarted_ = false;
        bool hasDirection_ = false;
        bool isGameOver_ = false;
        bool isPausedForRockBreak_ = false;

        float speedUpFactor_ = 0.5f; // 移動速度の加速係数
        float minMoveSpeed_ = 0.5f; // 最低移動速度
        float turnSlowdownFactor_ = 0.5f;
        float trainHeight_ = 1.0f;
        std::size_t requiredRailCount_ = 5;
    };
}

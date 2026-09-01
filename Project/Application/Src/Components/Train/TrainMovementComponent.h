#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents {
    class RailPathComponent;
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
            GameComponents::RailPathComponent* railPath = nullptr)
            : railPath_(railPath), gridSize_(gridSize), moveSpeed_(moveSpeed), gridX_(gridX), gridZ_(gridZ) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "TrainMovement";
        }

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // 発車後に進めるレールがなくなったか
        bool IsGameOver() const;

        // グリッドサイズを設定する
        void SetGridSize(float size);

    private:
        // 未確定レールを次の目的地として保存し、そのレールを確定する
        bool BeginNextSegment();
        // 現在の移動進捗を Transform に反映する
        void SyncTransformToProgress();
        // 移動方向に合わせて Y 軸回転を更新する
        void UpdateRotation();

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;
        float gridSize_ = 5.0f;

        float moveSpeed_ = 0.5f; // 移動速度（グリッド単位/秒）
        int32_t gridX_ = 0; // 現在のグリッドX座標
        int32_t gridZ_ = 0; // 現在のグリッドZ座標
        int32_t destinationGridX_ = 0;
        int32_t destinationGridZ_ = 0;

        float movementProgress_ = 0.0f;
        bool isMoving_ = false;
        bool hasStarted_ = false;
        bool hasDirection_ = false;
        bool isGameOver_ = false;

        float speedUpFactor_ = 0.5f; // 移動速度の加速係数
        float minMoveSpeed_ = 0.5f; // 最低移動速度

        static constexpr std::size_t kRequiredRailCount = 5;
    };
}

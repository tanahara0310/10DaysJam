#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstdint>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents
{
    class RailPathComponent;
    class RailResourceManagerComponent;
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
            GameComponents::RailResourceManagerComponent* resourceManager = nullptr)
            : gridSize_(gridSize), gridPosX_(gridPosX), gridPosZ_(gridPosZ), railPath_(railPath), resourceManager_(resourceManager) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "RailBuilder";
        }

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

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;
        GameComponents::RailResourceManagerComponent* resourceManager_ = nullptr;

        // 左・後ろ方向へ移動したときの符号なし整数アンダーフローを避ける
        int32_t gridPosX_ = 0;
        int32_t gridPosZ_ = 0;

        float gridSize_ = 5.0f;
        bool HorizontalPrioritize = true;

        float undoPushTimer_ = 0.0f;
        float undoPushMaxTime_ = 0.3f;
        float undoInterval_ = 0.05f;
        float undoIntervalTimer_ = 0.0f;
    };
}

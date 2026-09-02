#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstdint>

#include "MapChipData.h"

namespace GameComponents {
    class MapGeneratorComponent;
    class ModelRenderPoolComponent;
    class CameraManagerComponent;
}

namespace GameComponents
{
    // マップを生成するコンポーネント
    class MapViewComponent final
        : public CoreEngine::IComponent {
    public:
        explicit MapViewComponent(
            MapGeneratorComponent* mapGenerator,
            ModelRenderPoolComponent* groundRenderPool,
            ModelRenderPoolComponent* stationRenderPool,
            ModelRenderPoolComponent* rockRenderPool,
            CameraManagerComponent* cameraManager,
            float gridSize = 1.0f, uint32_t viewDistanceX = 30)
            : gridSize_(gridSize), viewDistanceX_(viewDistanceX),
            mapGenerator_(mapGenerator),
            groundRenderPool_(groundRenderPool),
            stationRenderPool_(stationRenderPool),
            rockRenderPool_(rockRenderPool),
            cameraManager_(cameraManager) {}

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "MapView";
        }

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // ビューの中心X座標を設定する
        void SetViewCenterX(uint32_t centerX) { mapViewCenterX_ = centerX; }
        // ビューの表示距離Xを設定する
        void SetViewDistanceX(uint32_t distanceX) { viewDistanceX_ = distanceX; }

    private:
        float gridSize_ = 1.0f;

        uint32_t mapViewCenterX_ = 0;
        uint32_t viewDistanceX_ = 30;

        MapGeneratorComponent* mapGenerator_ = nullptr;
        ModelRenderPoolComponent* groundRenderPool_ = nullptr;
        ModelRenderPoolComponent* stationRenderPool_ = nullptr;
        ModelRenderPoolComponent* rockRenderPool_ = nullptr;
        CameraManagerComponent* cameraManager_ = nullptr;

    };
}

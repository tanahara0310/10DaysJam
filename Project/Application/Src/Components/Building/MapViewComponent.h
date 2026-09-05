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
            ModelRenderPoolComponent* waterRenderPool,
            ModelRenderPoolComponent* stationRenderPool,
            ModelRenderPoolComponent* rockRenderPool,
            ModelRenderPoolComponent* bananaTreeRenderPool,
            CameraManagerComponent* cameraManager,
            float gridSize = 1.0f, uint32_t viewDistanceX = 30)
            : gridSize_(gridSize), viewDistanceX_(viewDistanceX),
            mapGenerator_(mapGenerator),
            groundRenderPool_(groundRenderPool),
            waterRenderPool_(waterRenderPool),
            stationRenderPool_(stationRenderPool),
            rockRenderPool_(rockRenderPool),
            bananaTreeRenderPool_(bananaTreeRenderPool),
            cameraManager_(cameraManager) {}

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "MapView";
        }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "マップ描画"; }
        bool DrawInspector() override;
#endif

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
        float groundHeight_ = -0.5f;
        CoreEngine::Vector3 groundScale_ = { 0.6f, 0.6f, 0.6f };
        float waterHeight_ = 0.0f;
        CoreEngine::Vector3 waterScale_ = { 1.0f, 0.3f, 1.0f };
        float stationHeight_ = 0.7f;
        CoreEngine::Vector3 stationScale_ = { 0.5f, 0.5f, 0.5f };
        float rockHeight_ = 0.7f;
        CoreEngine::Vector3 rockScale_ = { 0.7f, 0.7f, 0.7f };
        float bananaTreeHeight_ = 0.5f;
        CoreEngine::Vector3 bananaTreeScale_ = { 1.0f, 1.0f, 1.0f };

        MapGeneratorComponent* mapGenerator_ = nullptr;
        ModelRenderPoolComponent* groundRenderPool_ = nullptr;
        ModelRenderPoolComponent* waterRenderPool_ = nullptr;
        ModelRenderPoolComponent* stationRenderPool_ = nullptr;
        ModelRenderPoolComponent* rockRenderPool_ = nullptr;
        ModelRenderPoolComponent* bananaTreeRenderPool_ = nullptr;
        CameraManagerComponent* cameraManager_ = nullptr;

    };
}

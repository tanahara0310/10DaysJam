#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector4.h"
#include "Math/Vector/Vector3.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "MapChipData.h"

namespace CoreEngine {
    class Camera;
    class MsdfFont;
    class Text3DObject;
}

namespace GameComponents {
    class MapGeneratorComponent;
    class ModelRenderPoolComponent;
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
            ModelRenderPoolComponent* hardRockRenderPool,
            ModelRenderPoolComponent* bananaTreeRenderPool,
            ModelRenderPoolComponent* grassRenderPool,
            CoreEngine::Camera* viewCamera,
            float gridSize = 1.0f, uint32_t viewDistanceX = 30)
            : gridSize_(gridSize), viewDistanceX_(viewDistanceX),
            mapGenerator_(mapGenerator),
            groundRenderPool_(groundRenderPool),
            waterRenderPool_(waterRenderPool),
            stationRenderPool_(stationRenderPool),
            rockRenderPool_(rockRenderPool),
            hardRockRenderPool_(hardRockRenderPool),
            bananaTreeRenderPool_(bananaTreeRenderPool),
            grassRenderPool_(grassRenderPool),
            viewCamera_(viewCamera) {}

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
        void OnDestroy() override;

        // ビューの中心X座標を設定する
        void SetViewCenterX(uint32_t centerX) { mapViewCenterX_ = centerX; }
        // ビューの表示距離Xを設定する
        void SetViewDistanceX(uint32_t distanceX) { viewDistanceX_ = distanceX; }

        // サルを送り出した駅を1回だけ弾ませる。引数は駅チップのマス座標
        void PlayStationPop(int32_t gridX, int32_t gridZ);

        // バナナを収穫された木を、取られた向きへしならせて揺らす。
        // toward はサルが木のどちら側にいるかで、マス座標の差をそのまま渡してよい。
        // 同じ木が続けて取られたら、重ねずに頭から鳴らし直す。
        void PlayBananaTreeShake(int32_t gridX, int32_t gridZ, float towardX, float towardZ);

    private:
        // 再生中の駅の演出
        struct StationPop {
            int32_t gridX = 0;
            int32_t gridZ = 0;
            float elapsed = 0.0f;
        };

        // 再生中のバナナの木の演出
        struct BananaTreeShake {
            int32_t gridX = 0;
            int32_t gridZ = 0;
            float towardX = 0.0f; // サルがいる向き。木の +X 側にいるなら 1
            float towardZ = 0.0f;
            float elapsed = 0.0f;
        };

        // 地形と同じ描画範囲で、5mごとの距離目盛りを表示・再利用する。
        void UpdateDistanceMarkers(std::size_t startX, std::size_t endX);
        // 駅の演出を進め、終わったものを捨てる
        void UpdateStationPops(float deltaTime);
        // 指定マスの駅に掛ける拡縮を求める。演出していなければ等倍
        CoreEngine::Vector3 GetStationPopScale(std::size_t x, std::size_t z) const;
        // バナナの木の演出を進め、終わったものを捨てる
        void UpdateBananaTreeShakes(float deltaTime);
        // 指定マスのバナナの木のしなりを、渡された回転と拡縮へ反映する。
        // 演出していなければ何もしない
        void ApplyBananaTreeShake(std::size_t x, std::size_t z,
            CoreEngine::Vector3& rotate, CoreEngine::Vector3& scale) const;

        // マスごとの色ムラを求める。地面が一色だとマス目が読めないので、
        // ベースカラーへ掛ける係数をマス単位でわずかに散らす。
        CoreEngine::Vector4 CalcGroundTint(
            std::size_t x, std::size_t z, float cameraDistance) const;

        float gridSize_ = 1.0f;

        // ===== 地面の色ムラ =====
        // 明度のふり幅（ベースカラーへの乗算）。0 で従来どおりの一色。
        float groundTintStrength_ = 0.20f;
        // 明るいマスは青寄り、暗いマスは黄寄りへずらす量。明度だけだと白黒のムラに見える。
        float groundTintHueSwing_ = 0.12f;
        // この距離[m]を超えたらムラを弱め始める。
        float groundTintFadeStart_ = 16.0f;
        // フェード開始からムラが 0 になるまでの距離[m]。遠景のちらつき対策。
        float groundTintFadeRange_ = 22.0f;

        uint32_t mapViewCenterX_ = 0;
        uint32_t viewDistanceX_ = 30;

        MapGeneratorComponent* mapGenerator_ = nullptr;
        ModelRenderPoolComponent* groundRenderPool_ = nullptr;
        ModelRenderPoolComponent* waterRenderPool_ = nullptr;
        ModelRenderPoolComponent* stationRenderPool_ = nullptr;
        ModelRenderPoolComponent* rockRenderPool_ = nullptr;
        // レールを敷けない空白マスへ立てる、壊せない岩
        ModelRenderPoolComponent* hardRockRenderPool_ = nullptr;
        ModelRenderPoolComponent* bananaTreeRenderPool_ = nullptr;
        ModelRenderPoolComponent* grassRenderPool_ = nullptr;
        // 描画範囲はゲーム視点カメラの位置から決める（構図は CameraRig が握る）
        CoreEngine::Camera* viewCamera_ = nullptr;

        CoreEngine::MsdfFont* distanceMarkerFont_ = nullptr;
        std::vector<CoreEngine::Text3DObject*> distanceMarkers_;

        std::vector<StationPop> stationPops_;
        float stationPopDuration_ = 0.45f; // 沈んで跳ね返るまでの時間（秒）
        float stationPopSquash_ = 0.22f;   // 沈み込みの深さ（1.0 で高さが 0 になる）

        std::vector<BananaTreeShake> bananaTreeShakes_;
        float bananaTreeShakeDuration_ = 0.55f; // しなって戻り切るまでの時間（秒）
        float bananaTreeShakeLean_ = 0.17f;     // 取られた向きへ倒れる角度（ラジアン）
        float bananaTreeShakeSquash_ = 0.12f;   // しなりに合わせて縦へ縮む量

    };
}

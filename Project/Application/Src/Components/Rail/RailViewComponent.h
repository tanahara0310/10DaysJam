#pragma once

#include "Audio/SoundInstance.h"
#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <functional>

namespace CoreEngine
{
    class TransformComponent;
}

namespace GameComponents {
    class RailPathComponent;
    class ModelRenderPoolComponent;
    class CameraManagerComponent;
}

namespace GameComponents
{
    // 入力に応じてグリッド単位で移動するコンポーネント,レールパスが必要
    class RailViewComponent final
        : public CoreEngine::IComponent {
    public:
        explicit RailViewComponent(
            float gridSize = 5.0f,
            GameComponents::RailPathComponent* railPath = nullptr,
            GameComponents::ModelRenderPoolComponent* railPool = nullptr,
            GameComponents::ModelRenderPoolComponent* railLeftPool = nullptr,
            GameComponents::ModelRenderPoolComponent* railRightPool = nullptr,
            GameComponents::CameraManagerComponent* cameraManager = nullptr,
            std::function<void(float, float)> onRailBuildSE = nullptr,
            uint32_t viewDistanceX = 30)
            : gridSize_(gridSize), railPath_(railPath),
            railPool_(railPool),
            railLeftPool_(railLeftPool),
            railRightPool_(railRightPool),
            cameraManager_(cameraManager),
            viewDistanceX_(viewDistanceX),
            onRailBuildSE_(onRailBuildSE) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "RailView";
        }

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // グリッドサイズを設定する
        void SetGridSize(float size);
        // 中心位置を設定する
        void SetCenterPosition(const CoreEngine::Vector3& position);

    private:
        // 新しく確定したレールを、確定順に少しずつ遅らせて跳ねさせる
        void UpdateConfirmationAnimations(float deltaTime);
        float GetConfirmationJumpOffset(std::size_t railIndex) const;

        // レール経路を直線・左コーナー・右コーナーへ分類してモデルを描画する
        void DrawRailModels();

        CoreEngine::TransformComponent* transform_ = nullptr;
        GameComponents::RailPathComponent* railPath_ = nullptr;

        GameComponents::ModelRenderPoolComponent* railPool_ = nullptr;
        GameComponents::ModelRenderPoolComponent* railLeftPool_ = nullptr;
        GameComponents::ModelRenderPoolComponent* railRightPool_ = nullptr;
        GameComponents::CameraManagerComponent* cameraManager_ = nullptr;
        float gridSize_ = 5.0f;
        uint32_t viewDistanceX_ = 30;

        // railMap のインデックスと対応する。負値は再生開始までの待ち時間
        std::vector<float> confirmationAnimationTimes_;
        std::vector<float> confirmationSoundPitches_;

        // 引数は音量、ピッチの順
        std::function<void(float, float)> onRailBuildSE_ = nullptr;
    };
}

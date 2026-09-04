#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"

namespace CoreEngine
{
    class Camera;
    class TransformComponent;
}

namespace GameComponents
{
    // カメラの管理を行うコンポーネント
    class CameraManagerComponent final
        : public CoreEngine::IComponent {
    public:
        explicit CameraManagerComponent(
            CoreEngine::Camera* camera,
            CoreEngine::TransformComponent* trainTransform,
            CoreEngine::TransformComponent* builderTransform,
            float focusRatio = 0.5f,
            const CoreEngine::Vector3& cameraOffset = { 0.0f, 20.0f, -18.0f },
            float minTargetDistance = 5.0f,
            float maxTargetDistance = 30.0f,
            float minFovDegrees = 35.0f,
            float maxFovDegrees = 70.0f,
            float followSpeed = 3.0f)
            : camera_(camera),
              trainTransform_(trainTransform),
              builderTransform_(builderTransform),
              focusRatio_(focusRatio),
              cameraOffset_(cameraOffset),
              minTargetDistance_(minTargetDistance),
              maxTargetDistance_(maxTargetDistance),
              minFovDegrees_(minFovDegrees),
              maxFovDegrees_(maxFovDegrees),
              followSpeed_(followSpeed) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "CameraManager";
        }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "ゲームカメラ"; }
        bool DrawInspector() override;
#endif

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 全オブジェクトの移動後にカメラを更新する
        void LateUpdate() override;

        // カメラの位置と回転を取得する
        CoreEngine::Vector3 GetCameraPosition() const { return cameraPosition_; }
        // カメラの回転を取得する
        CoreEngine::Vector3 GetCameraRotation() const { return cameraRotation_; }
        // カメラのX位置を寄せる割合（0.0 = 列車側、0.5 = 中間、1.0 = ビルダー側）
        void SetFocusRatio(float ratio);
        float GetFocusRatio() const { return focusRatio_; }

        CoreEngine::Vector3 GetFocusPosition() const { return smoothedFocusPosition_; }
        float GetCurrentFovDegrees() const { return currentFovDegrees_; }

        // 現在の構図から列車のアップへ移動する。完了後もアップを維持する。
        void BeginTrainCloseUp(float duration = -1.0f, float distanceScale = -1.0f);
        bool IsTrainCloseUpComplete() const {
            return trainCloseUpActive_ && closeUpElapsed_ >= closeUpDuration_;
        }

    private:
        void UpdateTrainCloseUp();
        void ApplyCameraState();

        CoreEngine::Camera* camera_ = nullptr;
        CoreEngine::TransformComponent* trainTransform_ = nullptr;
        CoreEngine::TransformComponent* builderTransform_ = nullptr;

        float focusRatio_ = 0.5f;
        CoreEngine::Vector3 cameraOffset_ = { 0.0f, 30.0f, -25.0f };
        CoreEngine::Vector3 cameraPosition_ = { 0.0f, 0.0f, 0.0f };
        CoreEngine::Vector3 cameraRotation_ = { 0.0f, 0.0f, 0.0f };
        CoreEngine::Vector3 smoothedFocusPosition_ = { 0.0f, 0.0f, 0.0f };

        float minTargetDistance_ = 5.0f;
        float maxTargetDistance_ = 50.0f;
        float minFovDegrees_ = 35.0f;
        float maxFovDegrees_ = 70.0f;
        float followSpeed_ = 5.0f;
        float currentFovDegrees_ = 35.0f;

        bool trainCloseUpActive_ = false;
        float closeUpElapsed_ = 0.0f;
        float closeUpDuration_ = 1.5f;
        float closeUpDistanceScale_ = 0.3f;
        CoreEngine::Vector3 closeUpStartPosition_{};
        CoreEngine::Vector3 closeUpStartFocus_{};
        float closeUpStartFov_ = 35.0f;
    };
}

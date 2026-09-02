#include "pch.h"
#include "CameraManagerComponent.h"

#include "Camera/Camera.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

using namespace CoreEngine;

void GameComponents::CameraManagerComponent::Start() {
    if (!camera_ || !trainTransform_ || !builderTransform_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "CameraManagerComponent: Camera、Train、Builder のいずれかが未設定です");
        SetEnabled(false);
        return;
    }

    if (maxTargetDistance_ <= minTargetDistance_ ||
        maxFovDegrees_ < minFovDegrees_ || followSpeed_ <= 0.0f) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "CameraManagerComponent: 距離、FOV、追従速度の設定が不正です");
        SetEnabled(false);
        return;
    }

    cameraPosition_ = camera_->GetTranslate();
    cameraRotation_ = camera_->GetRotate();
    currentFovDegrees_ = camera_->GetParameters().GetFovDegrees();

    focusRatio_ = std::clamp(focusRatio_, 0.0f, 1.0f);
    const Vector3 trainPosition = trainTransform_->GetWorldPosition();
    const Vector3 builderPosition = builderTransform_->GetWorldPosition();
    smoothedFocusPosition_ = (trainPosition + builderPosition) * 0.5f;
}

void GameComponents::CameraManagerComponent::LateUpdate() {
    if (!camera_ || !trainTransform_ || !builderTransform_) {
        return;
    }

    const Vector3 trainPosition = trainTransform_->GetWorldPosition();
    const Vector3 builderPosition = builderTransform_->GetWorldPosition();
    const Vector3 desiredFocusPosition =
        (trainPosition + builderPosition) * 0.5f;
    const float targetDistance = Distance(trainPosition, builderPosition);

    const float distanceRate = std::clamp(
        (targetDistance - minTargetDistance_) /
        (maxTargetDistance_ - minTargetDistance_),
        0.0f,
        1.0f);
    const float desiredFovDegrees =
        minFovDegrees_ + (maxFovDegrees_ - minFovDegrees_) * distanceRate;

    // フレームレートに依存しにくい指数補間で、位置・注視点・視野角を滑らかに追従させる。
    const float interpolation =
        1.0f - std::exp(-followSpeed_ * Time::DeltaTime());
    Vector3 desiredCameraPosition = desiredFocusPosition + cameraOffset_;
    desiredCameraPosition.x =
        trainPosition.x * (1.0f - focusRatio_) +
        builderPosition.x * focusRatio_ +
        cameraOffset_.x;
    cameraPosition_ += (desiredCameraPosition - cameraPosition_) * interpolation;
    smoothedFocusPosition_ +=
        (desiredFocusPosition - smoothedFocusPosition_) * interpolation;
    currentFovDegrees_ +=
        (desiredFovDegrees - currentFovDegrees_) * interpolation;

    camera_->SetTranslate(cameraPosition_);
    camera_->LookAt(smoothedFocusPosition_);
    cameraRotation_ = camera_->GetRotate();

    CameraParameters parameters = camera_->GetParameters();
    parameters.SetFovDegrees(currentFovDegrees_);
    camera_->SetParameters(parameters);

    // CameraManager の通常更新後に値を変更するため、描画前に行列を反映する。
    camera_->UpdateMatrix();
}

void GameComponents::CameraManagerComponent::SetFocusRatio(float ratio) {
    focusRatio_ = std::clamp(ratio, 0.0f, 1.0f);
}

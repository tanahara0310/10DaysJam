#include "pch.h"
#include "CameraManagerComponent.h"

#include "Camera/Camera.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::CameraManagerComponent::OnSerialize() const {
    return {
        { "focusRatio", focusRatio_ },
        { "cameraOffset", JsonManager::Vector3ToJson(cameraOffset_) },
        { "minTargetDistance", minTargetDistance_ },
        { "maxTargetDistance", maxTargetDistance_ },
        { "minFovDegrees", minFovDegrees_ },
        { "maxFovDegrees", maxFovDegrees_ },
        { "followSpeed", followSpeed_ },
        { "closeUpDuration", closeUpDuration_ },
        { "closeUpDistanceScale", closeUpDistanceScale_ }
    };
}

void GameComponents::CameraManagerComponent::OnDeserialize(const json& j) {
    focusRatio_ = std::clamp(JsonManager::SafeGet<float>(j, "focusRatio", focusRatio_), 0.0f, 1.0f);
    cameraOffset_ = JsonManager::SafeGetVector3(j, "cameraOffset", cameraOffset_);
    minTargetDistance_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "minTargetDistance", minTargetDistance_));
    maxTargetDistance_ = std::max(minTargetDistance_ + 0.01f,
        JsonManager::SafeGet<float>(j, "maxTargetDistance", maxTargetDistance_));
    minFovDegrees_ = std::clamp(JsonManager::SafeGet<float>(j, "minFovDegrees", minFovDegrees_), 1.0f, 179.0f);
    maxFovDegrees_ = std::clamp(JsonManager::SafeGet<float>(j, "maxFovDegrees", maxFovDegrees_), minFovDegrees_, 179.0f);
    followSpeed_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "followSpeed", followSpeed_));
    closeUpDuration_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "closeUpDuration", closeUpDuration_));
    closeUpDistanceScale_ = std::clamp(
        JsonManager::SafeGet<float>(j, "closeUpDistanceScale", closeUpDistanceScale_), 0.1f, 1.0f);
}

#ifdef USE_IMGUI
bool GameComponents::CameraManagerComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::SliderFloat("注視比率", &focusRatio_, 0.0f, 1.0f);
    changed |= ImGui::DragFloat3("カメラオフセット", &cameraOffset_.x, 0.1f);
    changed |= ImGui::DragFloat("最小対象距離", &minTargetDistance_, 0.1f, 0.0f, maxTargetDistance_);
    changed |= ImGui::DragFloat("最大対象距離", &maxTargetDistance_, 0.1f, minTargetDistance_ + 0.01f, 300.0f);
    changed |= ImGui::SliderFloat("最小FOV", &minFovDegrees_, 1.0f, maxFovDegrees_);
    changed |= ImGui::SliderFloat("最大FOV", &maxFovDegrees_, minFovDegrees_, 179.0f);
    changed |= ImGui::DragFloat("追従速度", &followSpeed_, 0.05f, 0.01f, 30.0f);
    changed |= ImGui::DragFloat("クローズアップ時間", &closeUpDuration_, 0.05f, 0.0f, 10.0f);
    changed |= ImGui::SliderFloat("クローズアップ距離倍率", &closeUpDistanceScale_, 0.1f, 1.0f);
    return changed;
}
#endif

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

    if (trainCloseUpActive_) {
        UpdateTrainCloseUp();
        ApplyCameraState();
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

    ApplyCameraState();
}

void GameComponents::CameraManagerComponent::ApplyCameraState() {
    camera_->SetTranslate(cameraPosition_);
    camera_->LookAt(smoothedFocusPosition_);
    cameraRotation_ = camera_->GetRotate();

    CameraParameters parameters = camera_->GetParameters();
    parameters.SetFovDegrees(currentFovDegrees_);
    camera_->SetParameters(parameters);

    // CameraManager の通常更新後に値を変更するため、描画前に行列を反映する。
    camera_->UpdateMatrix();
}

void GameComponents::CameraManagerComponent::BeginTrainCloseUp(float duration, float distanceScale) {
    if (trainCloseUpActive_ || !IsEnabled() || !camera_ || !trainTransform_ || !builderTransform_) {
        return;
    }

    trainCloseUpActive_ = true;
    closeUpElapsed_ = 0.0f;
    if (duration >= 0.0f) {
        closeUpDuration_ = duration;
    }
    if (distanceScale >= 0.0f) {
        closeUpDistanceScale_ = std::clamp(distanceScale, 0.1f, 1.0f);
    }
    closeUpStartPosition_ = camera_->GetTranslate();
    closeUpStartFocus_ = smoothedFocusPosition_;
    closeUpStartFov_ = camera_->GetParameters().GetFovDegrees();
}

void GameComponents::CameraManagerComponent::UpdateTrainCloseUp() {
    closeUpElapsed_ = std::min(closeUpDuration_,
        closeUpElapsed_ + std::max(0.0f, Time::UnscaledDeltaTime()));
    const float t = closeUpDuration_ > 0.0f ? closeUpElapsed_ / closeUpDuration_ : 1.0f;
    // 開始・停止時の速度がゼロになる補間で、急な切り替わりを避ける。
    const float eased = t * t * (3.0f - 2.0f * t);
    const Vector3 focus = trainTransform_->GetWorldPosition() + Vector3{ 0.0f, 0.5f, 0.0f };
    const Vector3 position = focus + cameraOffset_ * closeUpDistanceScale_;
    const float fov = std::min(closeUpStartFov_, minFovDegrees_);
    cameraPosition_ = closeUpStartPosition_ + (position - closeUpStartPosition_) * eased;
    smoothedFocusPosition_ = closeUpStartFocus_ + (focus - closeUpStartFocus_) * eased;
    currentFovDegrees_ = closeUpStartFov_ + (fov - closeUpStartFov_) * eased;
}

void GameComponents::CameraManagerComponent::SetFocusRatio(float ratio) {
    focusRatio_ = std::clamp(ratio, 0.0f, 1.0f);
}

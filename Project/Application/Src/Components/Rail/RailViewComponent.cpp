#include "pch.h"
#include "RailViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/Utility/ModelRenderPoolComponent.h"
#include "Components/Camera/CameraManagerComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "Graphics/Line/LineManager.h"


using namespace CoreEngine;

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
}

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

json GameComponents::RailViewComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "viewDistanceX", viewDistanceX_ },
        { "railHeight", railHeight_ },
        { "railScale", railScale_ },
        { "jumpHeight", confirmationJumpHeight_ },
        { "jumpDuration", confirmationJumpDuration_ },
        { "staggerInterval", confirmationStaggerInterval_ },
        { "seVolume", confirmationSeVolume_ },
        { "seBasePitch", confirmationSeBasePitch_ },
        { "sePitchStep", confirmationSePitchStep_ },
        { "seMaxPitch", confirmationSeMaxPitch_ }
    };
}

void GameComponents::RailViewComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    viewDistanceX_ = std::max<uint32_t>(1, JsonManager::SafeGet<uint32_t>(j, "viewDistanceX", viewDistanceX_));
    railHeight_ = JsonManager::SafeGet<float>(j, "railHeight", railHeight_);
    railScale_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "railScale", railScale_));
    confirmationJumpHeight_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "jumpHeight", confirmationJumpHeight_));
    confirmationJumpDuration_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "jumpDuration", confirmationJumpDuration_));
    confirmationStaggerInterval_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "staggerInterval", confirmationStaggerInterval_));
    confirmationSeVolume_ = std::clamp(JsonManager::SafeGet<float>(j, "seVolume", confirmationSeVolume_), 0.0f, 1.0f);
    confirmationSeBasePitch_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "seBasePitch", confirmationSeBasePitch_));
    confirmationSePitchStep_ = std::max(0.0f, JsonManager::SafeGet<float>(j, "sePitchStep", confirmationSePitchStep_));
    confirmationSeMaxPitch_ = std::max(confirmationSeBasePitch_, JsonManager::SafeGet<float>(j, "seMaxPitch", confirmationSeMaxPitch_));
}

#ifdef USE_IMGUI
bool GameComponents::RailViewComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    int distance = static_cast<int>(viewDistanceX_);
    if (ImGui::DragInt("描画距離X", &distance, 1.0f, 1, 500)) { viewDistanceX_ = static_cast<uint32_t>(std::max(distance, 1)); changed = true; }
    changed |= ImGui::DragFloat("レール高さ", &railHeight_, 0.05f, -20.0f, 20.0f);
    changed |= ImGui::DragFloat("レールスケール", &railScale_, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("確定ジャンプ高さ", &confirmationJumpHeight_, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("確定ジャンプ時間", &confirmationJumpDuration_, 0.01f, 0.01f, 10.0f);
    changed |= ImGui::DragFloat("確定演出の時間差", &confirmationStaggerInterval_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::SliderFloat("確定SE音量", &confirmationSeVolume_, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("確定SE基準ピッチ", &confirmationSeBasePitch_, 0.01f, 0.01f, 4.0f);
    changed |= ImGui::DragFloat("確定SEピッチ増分", &confirmationSePitchStep_, 0.01f, 0.0f, 4.0f);
    changed |= ImGui::DragFloat("確定SE最大ピッチ", &confirmationSeMaxPitch_, 0.01f, confirmationSeBasePitch_, 4.0f);
    return changed;
}
#endif

void GameComponents::RailViewComponent::Start() {
    transform_ = Sibling<TransformComponent>();

    // ゲーム開始時から存在する始点レールは確定演出の対象にしない。
    if (railPath_) {
        confirmationAnimationTimes_.assign(
            railPath_->GetRailMap().size(),
            confirmationJumpDuration_);
        confirmationSoundPitches_.assign(
            railPath_->GetRailMap().size(),
            confirmationSeBasePitch_);
    }
}

void GameComponents::RailViewComponent::Update() {
    // TransformComponent がアタッチされていない場合は処理を中断する
    if (!transform_) {
        return;
    }
    // RailPathComponent がアタッチされていない場合は処理を中断する
    if (!railPath_) {
        return;
    }

    UpdateConfirmationAnimations(Time::DeltaTime());
    DrawRailModels();
    
    // LineManager のインスタンスを取得する
    auto& lines = LineManager::GetInstance();
    // 確定しているレールの座標を取得する
    const auto& railMap = railPath_->GetRailMap();
    for (size_t i = 0; i + 1 < railMap.size(); ++i) {
        float x = static_cast<float>(railMap[i].first) * gridSize_;
        float z = static_cast<float>(railMap[i].second) * gridSize_;

        float nextX = static_cast<float>(railMap[i + 1].first) * gridSize_;
        float nextZ = static_cast<float>(railMap[i + 1].second) * gridSize_;

        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 0.0f, 0.0f }, 1.0f, true);
    }

    // 確定していないレールの座標を取得する
    const auto& railUndoStack = railPath_->GetRailUndoStack();
    for (size_t i = 0; i + 1 < railUndoStack.size(); ++i) {
        float x = static_cast<float>(railUndoStack[i].first) * gridSize_;
        float z = static_cast<float>(railUndoStack[i].second) * gridSize_;

        float nextX = static_cast<float>(railUndoStack[i + 1].first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack[i + 1].second) * gridSize_;

        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 1.0f, 1.0f }, 1.0f, true);
    }

    // 確定したレールと確定していないレールの間のラインを描画する
    if (!railMap.empty() && !railUndoStack.empty()) {
        float x = static_cast<float>(railMap.back().first) * gridSize_;
        float z = static_cast<float>(railMap.back().second) * gridSize_;
        float nextX = static_cast<float>(railUndoStack.front().first) * gridSize_;
        float nextZ = static_cast<float>(railUndoStack.front().second) * gridSize_;
        // ラインを描画する
        lines.DrawLine({ x, 1.0f, z }, { nextX, 1.0f, nextZ }, { 1.0f, 1.0f, 0.0f }, 1.0f, true);
    }
}

void GameComponents::RailViewComponent::UpdateConfirmationAnimations(float deltaTime) {
    const std::size_t confirmedCount = railPath_->GetRailMap().size();

    // 将来確定済みレールを巻き戻す処理が追加されても、添字を範囲内に保つ。
    if (confirmationAnimationTimes_.size() > confirmedCount) {
        confirmationAnimationTimes_.resize(confirmedCount);
        confirmationSoundPitches_.resize(confirmedCount);
    }

    // 同じフレームに複数本確定した場合（駅到達時）は、順番に再生する。
    const std::size_t firstNewIndex = confirmationAnimationTimes_.size();
    for (std::size_t i = firstNewIndex; i < confirmedCount; ++i) {
        const float delay = confirmationStaggerInterval_ *
            static_cast<float>(i - firstNewIndex);
        confirmationAnimationTimes_.push_back(-delay);
        confirmationSoundPitches_.push_back(std::min(
            confirmationSeBasePitch_ +
                confirmationSePitchStep_ * static_cast<float>(i - firstNewIndex),
            confirmationSeMaxPitch_));
    }

    const float safeDeltaTime = std::max(deltaTime, 0.0f);
    for (std::size_t i = 0; i < confirmationAnimationTimes_.size(); ++i) {
        float& animationTime = confirmationAnimationTimes_[i];
        if (animationTime < confirmationJumpDuration_) {
            const float previousTime = animationTime;
            animationTime = std::min(
                animationTime + safeDeltaTime,
                confirmationJumpDuration_);

            // 待ち時間を越えてレールが跳ね始める瞬間に、一度だけSEを鳴らす。
            if (previousTime <= 0.0f && animationTime > 0.0f && onRailBuildSE_) {
                onRailBuildSE_(confirmationSeVolume_, confirmationSoundPitches_[i]);
            }
        }
    }
}

float GameComponents::RailViewComponent::GetConfirmationJumpOffset(
    std::size_t railIndex) const {
    if (railIndex >= confirmationAnimationTimes_.size()) {
        return 0.0f;
    }

    const float animationTime = confirmationAnimationTimes_[railIndex];
    if (animationTime < 0.0f || animationTime >= confirmationJumpDuration_) {
        return 0.0f;
    }

    const float progress = std::clamp(
        animationTime / confirmationJumpDuration_,
        0.0f,
        1.0f);
    return std::sin(progress * kPi) * confirmationJumpHeight_;
}

void GameComponents::RailViewComponent::DrawRailModels() {
    if (!railPool_ || !railLeftPool_ || !railRightPool_) {
        return;
    }

    using GridPosition = std::pair<int32_t, int32_t>;

    // 確定・未確定を連結し、1本の経路としてモデルの形状を判定する。
    const auto& confirmedRails = railPath_->GetRailMap();
    const auto& pendingRails = railPath_->GetRailUndoStack();
    std::vector<GridPosition> railPath;
    railPath.reserve(confirmedRails.size() + pendingRails.size());
    railPath.insert(railPath.end(), confirmedRails.begin(), confirmedRails.end());
    railPath.insert(railPath.end(), pendingRails.begin(), pendingRails.end());

    if (railPath.empty()) {
        return;
    }

    int32_t minVisibleX = 0;
    int32_t maxVisibleX = (std::numeric_limits<int32_t>::max)();
    if (cameraManager_ && gridSize_ > 0.0f) {
        const float centerGridX =
            cameraManager_->GetFocusPosition().x / gridSize_;
        minVisibleX = std::max(
            0,
            static_cast<int32_t>(std::floor(centerGridX)) -
                static_cast<int32_t>(viewDistanceX_));
        maxVisibleX =
            static_cast<int32_t>(std::ceil(centerGridX)) +
            static_cast<int32_t>(viewDistanceX_);
    }

    const auto directionBetween = [](const GridPosition& from, const GridPosition& to) {
        return GridPosition{ to.first - from.first, to.second - from.second };
    };
    const auto yawFromDirection = [](const GridPosition& direction) {
        // モデルの前方が+Zなので、+Zを0ラジアンとしてY軸回転を求める。
        return std::atan2(
            static_cast<float>(direction.first),
            static_cast<float>(direction.second));
    };

    for (std::size_t i = 0; i < railPath.size(); ++i) {
        const GridPosition current = railPath[i];
        if (current.first < minVisibleX || current.first > maxVisibleX) {
            continue;
        }

        const bool hasPrevious = i > 0;
        const bool hasNext = i + 1 < railPath.size();
        GridPosition incoming = { 0, 1 };
        GridPosition outgoing = { 0, 1 };

        if (hasPrevious) {
            incoming = directionBetween(railPath[i - 1], current);
        }
        if (hasNext) {
            outgoing = directionBetween(current, railPath[i + 1]);
        }
        if (!hasPrevious && hasNext) {
            incoming = outgoing;
        } else if (hasPrevious && !hasNext) {
            outgoing = incoming;
        }

        const float jumpOffset = i < confirmedRails.size()
            ? GetConfirmationJumpOffset(i)
            : 0.0f;
        const Vector3 position = {
            static_cast<float>(current.first) * gridSize_,
            railHeight_ + jumpOffset,
            static_cast<float>(current.second) * gridSize_
        };

        float scaleOffset = railScale_;
        const Vector3 scale = { scaleOffset, scaleOffset, scaleOffset };

        // XZ平面の外積。正なら進行方向に対して左折、負なら右折。
        const int32_t turn =
            incoming.first * outgoing.second -
            incoming.second * outgoing.first;
        if (hasPrevious && hasNext && turn > 0) {
            railLeftPool_->Draw(
                position, { 0.0f, yawFromDirection(incoming), 0.0f }, scale);
        } else if (hasPrevious && hasNext && turn < 0) {
            railRightPool_->Draw(
                position, { 0.0f, yawFromDirection(incoming), 0.0f }, scale);
        } else {
            railPool_->Draw(
                position, { 0.0f, yawFromDirection(outgoing), 0.0f }, scale);
        }
    }
}

void GameComponents::RailViewComponent::SetGridSize(float size) {
    gridSize_ = size;
}

void GameComponents::RailViewComponent::SetCenterPosition(const CoreEngine::Vector3& position) {
    transform_->Get().translate = position;
}

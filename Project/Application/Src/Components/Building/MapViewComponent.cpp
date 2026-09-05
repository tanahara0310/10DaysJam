#include "pch.h"
#include "MapViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "MapGeneratorComponent.h"
#include "Components/Utility/ModelRenderPoolComponent.h"
#include "Camera/Camera.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::MapViewComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "viewDistanceX", viewDistanceX_ },
        { "groundHeight", groundHeight_ },
        { "groundScale", JsonManager::Vector3ToJson(groundScale_) },
        { "waterHeight", waterHeight_ },
        { "waterScale", JsonManager::Vector3ToJson(waterScale_) },
        { "stationHeight", stationHeight_ },
        { "stationScale", JsonManager::Vector3ToJson(stationScale_) },
        { "rockHeight", rockHeight_ },
        { "rockScale", JsonManager::Vector3ToJson(rockScale_) },
        { "bananaTreeHeight", bananaTreeHeight_ },
        { "bananaTreeScale", JsonManager::Vector3ToJson(bananaTreeScale_) }
    };
}

void GameComponents::MapViewComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    viewDistanceX_ = std::max<uint32_t>(1, JsonManager::SafeGet<uint32_t>(j, "viewDistanceX", viewDistanceX_));
    groundHeight_ = JsonManager::SafeGet<float>(j, "groundHeight", groundHeight_);
    groundScale_ = JsonManager::SafeGetVector3(j, "groundScale", groundScale_);
    waterHeight_ = JsonManager::SafeGet<float>(j, "waterHeight", waterHeight_);
    waterScale_ = JsonManager::SafeGetVector3(j, "waterScale", waterScale_);
    stationHeight_ = JsonManager::SafeGet<float>(j, "stationHeight", stationHeight_);
    stationScale_ = JsonManager::SafeGetVector3(j, "stationScale", stationScale_);
    rockHeight_ = JsonManager::SafeGet<float>(j, "rockHeight", rockHeight_);
    rockScale_ = JsonManager::SafeGetVector3(j, "rockScale", rockScale_);
    bananaTreeHeight_ = JsonManager::SafeGet<float>(j, "bananaTreeHeight", bananaTreeHeight_);
    bananaTreeScale_ = JsonManager::SafeGetVector3(j, "bananaTreeScale", bananaTreeScale_);
}

#ifdef USE_IMGUI
bool GameComponents::MapViewComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    int distance = static_cast<int>(viewDistanceX_);
    if (ImGui::DragInt("描画距離X", &distance, 1.0f, 1, 500)) { viewDistanceX_ = static_cast<uint32_t>(std::max(distance, 1)); changed = true; }
    changed |= ImGui::DragFloat("地面の高さ", &groundHeight_, 0.05f);
    changed |= ImGui::DragFloat3("地面スケール", &groundScale_.x, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("水面の高さ", &waterHeight_, 0.05f);
    changed |= ImGui::DragFloat3("水面スケール", &waterScale_.x, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("駅の高さ", &stationHeight_, 0.05f);
    changed |= ImGui::DragFloat3("駅スケール", &stationScale_.x, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("岩の高さ", &rockHeight_, 0.05f);
    changed |= ImGui::DragFloat3("岩スケール", &rockScale_.x, 0.01f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat("バナナの木の高さ", &bananaTreeHeight_, 0.05f);
    changed |= ImGui::DragFloat3("バナナの木スケール", &bananaTreeScale_.x, 0.01f, 0.0f, 10.0f);
    return changed;
}
#endif

void GameComponents::MapViewComponent::Start() {
}

void GameComponents::MapViewComponent::Update() {
    // マップジェネレーターとグラウンドレンダープールが有効か確認する
    if (mapGenerator_ == nullptr || groundRenderPool_ == nullptr || viewCamera_ == nullptr) {
        return;
    }

    // カメラの注視位置を取得する
    const auto cameraFocusPosition = viewCamera_->GetTranslate();
    const float cameraFocusGridX = std::round(cameraFocusPosition.x / gridSize_);
    mapViewCenterX_ = cameraFocusGridX > 0.0f
        ? static_cast<uint32_t>(cameraFocusGridX)
        : 0;

    // 描画する範囲を決定する
    size_t startX = (mapViewCenterX_ > viewDistanceX_) ? (mapViewCenterX_ - viewDistanceX_) : 0;
    size_t endX = mapViewCenterX_ + viewDistanceX_;

    // カメラの先に必要な分だけ、X正方向へマップを延長する
    mapGenerator_->CreateToX(endX);

    Vector3 rotate{ 0.0f, 0.0f, 0.0f };

    // マップチップの2D配列を取得する
    const auto& mapChips = mapGenerator_->GetMapChips();
    // 描画範囲内のマップチップを描画する
    for (size_t x = startX; x < endX && x < mapChips.size(); ++x) {
        for (size_t z = 0; z < mapChips[x].size(); ++z) {
            const auto& chipType = mapChips[x][z];
            // チップの種類に応じて描画する
            if (chipType != MapChipType::Void && chipType != MapChipType::Water) {
                // グラウンドチップの表示
                groundRenderPool_->Draw({ x * gridSize_, groundHeight_, z * gridSize_ }, rotate, groundScale_);
            }

            // 水場チップの表示
            if (waterRenderPool_ && chipType == MapChipType::Water) {
                waterRenderPool_->Draw(
                    { x * gridSize_, waterHeight_, z * gridSize_ },
                    rotate,
                    waterScale_);
            }

            // 駅チップの表示
            if(stationRenderPool_ && chipType == MapChipType::Station) {
                stationRenderPool_->Draw({ x * gridSize_, stationHeight_, z * gridSize_ }, rotate, stationScale_);
            }

            // 岩チップの表示
            if (rockRenderPool_ && chipType == MapChipType::Resource) {
                rockRenderPool_->Draw({ x * gridSize_, rockHeight_, z * gridSize_ }, rotate, rockScale_);
            }

            // バナナの木チップの表示
            if (bananaTreeRenderPool_ && chipType == MapChipType::BananaTree) {
                bananaTreeRenderPool_->Draw(
                    { x * gridSize_, bananaTreeHeight_, z * gridSize_ },
                    rotate,
                    bananaTreeScale_);
            }
        }
    }
}

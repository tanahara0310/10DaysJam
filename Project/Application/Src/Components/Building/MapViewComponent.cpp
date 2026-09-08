#include "pch.h"
#include "MapViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "GameObject/Text3D/Text3DObject.h"
#include "MapGeneratorComponent.h"
#include "Components/Utility/BlockModelLayout.h"
#include "Components/Utility/ModelRenderPoolComponent.h"
#include "Camera/Camera.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Text/FontManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

namespace {
    constexpr std::size_t kDistanceMarkerIntervalMeters = 5;
    constexpr float kDistanceMarkerFontSize = 0.45f;
}

json GameComponents::MapViewComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "viewDistanceX", viewDistanceX_ },
        { "stationPopDuration", stationPopDuration_ },
        { "stationPopSquash", stationPopSquash_ }
    };
}

void GameComponents::MapViewComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    viewDistanceX_ = std::max<uint32_t>(1, JsonManager::SafeGet<uint32_t>(j, "viewDistanceX", viewDistanceX_));
    stationPopDuration_ = std::max(0.01f,
        JsonManager::SafeGet<float>(j, "stationPopDuration", stationPopDuration_));
    stationPopSquash_ = std::clamp(
        JsonManager::SafeGet<float>(j, "stationPopSquash", stationPopSquash_), 0.0f, 1.0f);
    // 旧データのモデル別高さ・スケールは使わず、共通ブロック寸法から求める。
}

#ifdef USE_IMGUI
bool GameComponents::MapViewComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("グリッドサイズ", &gridSize_, 0.05f, 0.01f, 20.0f);
    int distance = static_cast<int>(viewDistanceX_);
    if (ImGui::DragInt("描画距離X", &distance, 1.0f, 1, 500)) { viewDistanceX_ = static_cast<uint32_t>(std::max(distance, 1)); changed = true; }
    ImGui::TextDisabled("共通モデルスケール: %.3f", BlockModelLayout::GetScale(gridSize_));
    ImGui::TextDisabled("接地面の高さ: %.3f", BlockModelLayout::GetSurfaceHeight(gridSize_));
    ImGui::SeparatorText("駅の出現演出");
    changed |= ImGui::DragFloat("駅の反動時間", &stationPopDuration_, 0.01f, 0.01f, 5.0f);
    changed |= ImGui::SliderFloat("駅の沈み込み", &stationPopSquash_, 0.0f, 1.0f);
    return changed;
}
#endif

void GameComponents::MapViewComponent::PlayStationPop(int32_t gridX, int32_t gridZ) {
    // 同じ駅が続けて鳴ったら、重ねずに頭から鳴らし直す。
    for (auto& pop : stationPops_) {
        if (pop.gridX == gridX && pop.gridZ == gridZ) {
            pop.elapsed = 0.0f;
            return;
        }
    }
    stationPops_.push_back({ gridX, gridZ, 0.0f });
}

void GameComponents::MapViewComponent::UpdateStationPops(float deltaTime) {
    const float safeDeltaTime = std::max(deltaTime, 0.0f);
    for (auto& pop : stationPops_) {
        pop.elapsed += safeDeltaTime;
    }
    std::erase_if(stationPops_, [this](const StationPop& pop) {
        return pop.elapsed >= stationPopDuration_;
    });
}

Vector3 GameComponents::MapViewComponent::GetStationPopScale(
    std::size_t x, std::size_t z) const {
    for (const auto& pop : stationPops_) {
        if (pop.gridX < 0 || pop.gridZ < 0 ||
            static_cast<std::size_t>(pop.gridX) != x ||
            static_cast<std::size_t>(pop.gridZ) != z) {
            continue;
        }

        // サルが飛び出した反動でいったん沈み、跳ね返って収まる。
        // 減衰する正弦波1周期ぶんで、前半が沈み込み、後半が伸び上がりになる。
        const float progress = std::clamp(
            pop.elapsed / stationPopDuration_, 0.0f, 1.0f);
        const float recoil =
            std::sin(progress * 2.0f * std::numbers::pi_v<float>) *
            (1.0f - progress) * stationPopSquash_;
        return { 1.0f + recoil * 0.6f, 1.0f - recoil, 1.0f + recoil * 0.6f };
    }
    return { 1.0f, 1.0f, 1.0f };
}

void GameComponents::MapViewComponent::Start() {
    auto* owner = GetOwner();
    auto* engine = owner ? owner->GetEngineSystem() : nullptr;
    auto* fontManager = engine ? engine->GetService<FontManager>() : nullptr;
    if (fontManager) {
        MsdfFontDesc fontDesc;
        fontDesc.filePath = L"Engine/Assets/font/x8y12pxDenkiChip.ttf";
        fontDesc.systemFamilyNames = { L"Segoe UI" };
        fontDesc.charsetUtf8 = "0123456789m|";
        distanceMarkerFont_ = fontManager->Acquire(fontDesc);
    }
    if (!distanceMarkerFont_) {
        Logger::GetInstance().Warnf(
            LogCategory::Game, "MapView: 距離目盛り用のフォントを取得できませんでした");
    }
}

void GameComponents::MapViewComponent::OnDestroy() {
    for (auto* marker : distanceMarkers_) {
        if (marker && !marker->IsMarkedForDestroy()) {
            marker->Destroy();
        }
    }
    distanceMarkers_.clear();
    distanceMarkerFont_ = nullptr;
}

void GameComponents::MapViewComponent::Update() {
    // マップジェネレーターとグラウンドレンダープールが有効か確認する
    if (mapGenerator_ == nullptr || groundRenderPool_ == nullptr || viewCamera_ == nullptr) {
        return;
    }

    UpdateStationPops(Time::DeltaTime());

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
    const float modelScale = BlockModelLayout::GetScale(gridSize_);
    const Vector3 scale{ modelScale, modelScale, modelScale };
    const float groundHeight = BlockModelLayout::GetGroundHeight(gridSize_);
    const float surfaceHeight = BlockModelLayout::GetSurfaceHeight(gridSize_);
    // 水だけは幅1・中心原点の仮モデル box.obj を、1マス幅の薄い水面にする。
    const Vector3 waterScale{ gridSize_, gridSize_ * 0.3f, gridSize_ };

    // マップチップの2D配列を取得する
    const auto& mapChips = mapGenerator_->GetMapChips();
    UpdateDistanceMarkers(startX, std::min(endX, mapChips.size()));
    // 描画範囲内のマップチップを描画する
    for (size_t x = startX; x < endX && x < mapChips.size(); ++x) {
        for (size_t z = 0; z < mapChips[x].size(); ++z) {
            const auto chipType = mapGenerator_->GetMapChip(x, z);
            // チップの種類に応じて描画する
            if (chipType != MapChipType::Void && chipType != MapChipType::Water) {
                // グラウンドチップの表示
                groundRenderPool_->Draw({ x * gridSize_, groundHeight, z * gridSize_ }, rotate, scale);
            }

            // 水場チップの表示
            if (waterRenderPool_ && chipType == MapChipType::Water) {
                waterRenderPool_->Draw(
                    { x * gridSize_, 0.0f, z * gridSize_ },
                    rotate,
                    waterScale);
            }

            // 駅チップの表示。サルを送り出した直後だけ反動で沈み込む。
            if(stationRenderPool_ && chipType == MapChipType::Station) {
                const Vector3 popScale = GetStationPopScale(x, z);
                stationRenderPool_->Draw(
                    { x * gridSize_, surfaceHeight, z * gridSize_ },
                    rotate,
                    { scale.x * popScale.x, scale.y * popScale.y, scale.z * popScale.z });
            }

            // 岩チップの表示
            if (rockRenderPool_ && chipType == MapChipType::Resource) {
                rockRenderPool_->Draw({ x * gridSize_, surfaceHeight, z * gridSize_ }, rotate, scale);
            }

            // バナナの木チップの表示
            if (bananaTreeRenderPool_ && chipType == MapChipType::BananaTree) {
                bananaTreeRenderPool_->Draw(
                    { x * gridSize_, surfaceHeight, z * gridSize_ },
                    rotate,
                    scale);
            }

            // 草は地面の上に重ねる装飾として描画する。
            if (grassRenderPool_ && chipType == MapChipType::Grass) {
                grassRenderPool_->Draw(
                    { x * gridSize_, surfaceHeight, z * gridSize_ },
                    rotate,
                    scale);
            }
        }
    }
}

void GameComponents::MapViewComponent::UpdateDistanceMarkers(
    std::size_t startX, std::size_t endX) {
    if (!distanceMarkerFont_) {
        return;
    }

    // X=0を0m、1ワールド単位を1mとする。マスの大きさを変えても間隔は5m。
    const double startMeters = static_cast<double>(startX) * gridSize_;
    const double endMeters = static_cast<double>(endX) * gridSize_;
    const std::size_t firstMarker = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(startMeters / kDistanceMarkerIntervalMeters)));
    std::size_t usedCount = 0;
    for (std::size_t markerIndex = firstMarker;
        static_cast<double>(markerIndex) * kDistanceMarkerIntervalMeters < endMeters;
        ++markerIndex) {
        if (usedCount == distanceMarkers_.size()) {
            auto* marker = GetOwner()->Spawn<Text3DObject>();
            if (!marker) {
                break;
            }
            marker->Initialize(distanceMarkerFont_, "", "DistanceMarker_" + std::to_string(usedCount));
            // 描画範囲に応じて再配置するため、個々の目盛りはシーンに保存しない。
            marker->SetSerializeEnabled(false);
            marker->SetAlign(TextAlignH::Center, TextAlignV::Top);
            marker->SetPivot({ 0.5f, 0.0f });
            marker->SetLineSpacing(0.9f);
            marker->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            marker->SetOutline({ 0.05f, 0.05f, 0.05f, 0.8f }, 0.025f);
            marker->SetBillboard(Text3DBillboard::None);
            marker->SetDepthMode(Text3DDepthMode::Test);
            distanceMarkers_.push_back(marker);
        }

        const std::size_t meters = markerIndex * kDistanceMarkerIntervalMeters;
        auto* marker = distanceMarkers_[usedCount++];
        marker->SetText("|\n" + std::to_string(meters) + "m");
        marker->SetFontSize(kDistanceMarkerFontSize);
        auto& transform = marker->GetComponent<TransformComponent>()->Get();
        // ゲームカメラ側（-Z）の地形端より外へ置く。文字面は床と平行で、上から読める向き。
        transform.translate = {
            static_cast<float>(meters),
            BlockModelLayout::GetSurfaceHeight(gridSize_) + 0.015f * gridSize_,
            -0.75f * gridSize_
        };
        transform.rotate = { std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f };
        transform.TransferMatrix();
        marker->SetActive(true);
    }

    // 戻ったり描画距離を縮めたりした場合は、余った目盛りを隠す。
    for (std::size_t i = usedCount; i < distanceMarkers_.size(); ++i) {
        distanceMarkers_[i]->SetActive(false);
    }
}

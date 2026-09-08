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
#include "Utility/Random/Hash.h"

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
        { "groundSkirtHeight", groundSkirtHeight_ },
        { "groundTintStrength", groundTintStrength_ },
        { "groundTintHueSwing", groundTintHueSwing_ },
        { "groundTintFadeStart", groundTintFadeStart_ },
        { "groundTintFadeRange", groundTintFadeRange_ },
        { "stationPopDuration", stationPopDuration_ },
        { "stationPopSquash", stationPopSquash_ },
        { "bananaTreeShakeDuration", bananaTreeShakeDuration_ },
        { "bananaTreeShakeLean", bananaTreeShakeLean_ },
        { "bananaTreeShakeSquash", bananaTreeShakeSquash_ }
    };
}

void GameComponents::MapViewComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    viewDistanceX_ = std::max<uint32_t>(1, JsonManager::SafeGet<uint32_t>(j, "viewDistanceX", viewDistanceX_));
    groundSkirtHeight_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "groundSkirtHeight", groundSkirtHeight_));
    groundTintStrength_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "groundTintStrength", groundTintStrength_));
    groundTintHueSwing_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "groundTintHueSwing", groundTintHueSwing_));
    groundTintFadeStart_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "groundTintFadeStart", groundTintFadeStart_));
    groundTintFadeRange_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "groundTintFadeRange", groundTintFadeRange_));
    stationPopDuration_ = std::max(0.01f,
        JsonManager::SafeGet<float>(j, "stationPopDuration", stationPopDuration_));
    stationPopSquash_ = std::clamp(
        JsonManager::SafeGet<float>(j, "stationPopSquash", stationPopSquash_), 0.0f, 1.0f);
    bananaTreeShakeDuration_ = std::max(0.01f,
        JsonManager::SafeGet<float>(j, "bananaTreeShakeDuration", bananaTreeShakeDuration_));
    bananaTreeShakeLean_ = std::clamp(
        JsonManager::SafeGet<float>(j, "bananaTreeShakeLean", bananaTreeShakeLean_), 0.0f, 1.5f);
    bananaTreeShakeSquash_ = std::clamp(
        JsonManager::SafeGet<float>(j, "bananaTreeShakeSquash", bananaTreeShakeSquash_), 0.0f, 1.0f);
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

    ImGui::SeparatorText("地面ブロックの伸ばし");
    changed |= ImGui::DragFloat("柱の長さ", &groundSkirtHeight_, 0.05f, 0.0f, 20.0f);
    ImGui::TextDisabled("柱の底: %.2f m",
        BlockModelLayout::GetGroundSkirtBottomHeight(gridSize_, groundSkirtHeight_));
    ImGui::TextDisabled("草が出始める高さ: %.2f m",
        BlockModelLayout::GetGroundSkirtGrassTopHeight(gridSize_, groundSkirtHeight_));
    ImGui::TextDisabled("上面は動かないので他のオブジェクトの高さは変わらない");
    ImGui::TextDisabled("「草が出始める高さ」より下で雲が不透明になっていないと、");
    ImGui::TextDisabled("逆さに吊るした草の緑が見えてしまう（ゲーム設定の Game.Fog.*）");

    ImGui::SeparatorText("地面の色ムラ");
    changed |= ImGui::DragFloat("明度のふり幅", &groundTintStrength_, 0.005f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("色味のふり幅", &groundTintHueSwing_, 0.005f, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("フェード開始距離", &groundTintFadeStart_, 0.5f, 0.0f, 300.0f);
    changed |= ImGui::DragFloat("フェード距離", &groundTintFadeRange_, 0.5f, 0.0f, 300.0f);
    ImGui::TextDisabled("0 にすると従来どおりの一色になる");
    ImGui::SeparatorText("駅の出現演出");
    changed |= ImGui::DragFloat("駅の反動時間", &stationPopDuration_, 0.01f, 0.01f, 5.0f);
    changed |= ImGui::SliderFloat("駅の沈み込み", &stationPopSquash_, 0.0f, 1.0f);
    ImGui::SeparatorText("バナナの木の収穫演出");
    changed |= ImGui::DragFloat("しなりの時間", &bananaTreeShakeDuration_, 0.01f, 0.01f, 5.0f);
    changed |= ImGui::SliderFloat("しなりの角度", &bananaTreeShakeLean_, 0.0f, 1.5f);
    changed |= ImGui::SliderFloat("しなりの縮み", &bananaTreeShakeSquash_, 0.0f, 1.0f);
    ImGui::TextDisabled("サルが取った向きへ倒れて、1.5往復しながら収まる");
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

void GameComponents::MapViewComponent::PlayBananaTreeShake(
    int32_t gridX, int32_t gridZ, float towardX, float towardZ) {
    // 同じ木が続けて取られたら、重ねずに頭から鳴らし直す。
    // 列車が長いと後続のサルが同じ木を次々に取るので、ここは頻繁に通る。
    for (auto& shake : bananaTreeShakes_) {
        if (shake.gridX == gridX && shake.gridZ == gridZ) {
            shake.towardX = towardX;
            shake.towardZ = towardZ;
            shake.elapsed = 0.0f;
            return;
        }
    }
    bananaTreeShakes_.push_back({ gridX, gridZ, towardX, towardZ, 0.0f });
}

void GameComponents::MapViewComponent::UpdateBananaTreeShakes(float deltaTime) {
    const float safeDeltaTime = std::max(deltaTime, 0.0f);
    for (auto& shake : bananaTreeShakes_) {
        shake.elapsed += safeDeltaTime;
    }
    std::erase_if(bananaTreeShakes_, [this](const BananaTreeShake& shake) {
        return shake.elapsed >= bananaTreeShakeDuration_;
    });
}

void GameComponents::MapViewComponent::ApplyBananaTreeShake(
    std::size_t x, std::size_t z, Vector3& rotate, Vector3& scale) const {
    for (const auto& shake : bananaTreeShakes_) {
        if (shake.gridX < 0 || shake.gridZ < 0 ||
            static_cast<std::size_t>(shake.gridX) != x ||
            static_cast<std::size_t>(shake.gridZ) != z) {
            continue;
        }

        // もぎ取られた向きへ大きくしなり、跳ね返りながら収まる。
        // 減衰する正弦波1.5周期ぶんで、最初の山がサル側への倒れ込みになる。
        const float progress = std::clamp(shake.elapsed / bananaTreeShakeDuration_, 0.0f, 1.0f);
        const float decay = (1.0f - progress) * (1.0f - progress);
        const float wave =
            std::sin(progress * 3.0f * std::numbers::pi_v<float>) * decay;

        // モデルの原点は幹の根元なので、回転させると木がそこを支点に倒れる。
        // 左手系では X 軸回転が +Y を +Z へ、Z 軸回転が +Y を -X へ倒す。
        const float lean = wave * bananaTreeShakeLean_;
        rotate.x += shake.towardZ * lean;
        rotate.z += -shake.towardX * lean;

        // 倒れる向きに関わらず、しなっている間は縦に縮んで横へ広がる。
        const float squash = std::abs(wave) * bananaTreeShakeSquash_;
        scale.y *= 1.0f - squash;
        scale.x *= 1.0f + squash * 0.5f;
        scale.z *= 1.0f + squash * 0.5f;
        return;
    }
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
    UpdateBananaTreeShakes(Time::DeltaTime());

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

    // 地面ブロックの下へ吊るす柱（スカート）。地面と同じ位置・同じ色のまま、
    // 上下逆さにして底面からぶら下げる。ブロックの上面は動かさない。
    // 逆さにするのは ground.obj の草（緑）を柱の最下部へ回すため。詳細は
    // BlockModelLayout.h の「地面ブロックのスカート」を見ること。
    const bool drawGroundSkirt = groundSkirtRenderPool_ && groundSkirtHeight_ > 0.0f;
    const Vector3 groundSkirtRotate{ std::numbers::pi_v<float>, 0.0f, 0.0f };
    const Vector3 groundSkirtScale{
        modelScale,
        BlockModelLayout::GetGroundSkirtYScale(gridSize_, groundSkirtHeight_),
        modelScale };
    // 水だけは幅1・中心原点の仮モデル box.obj を、1マス幅の薄い水面にする。
    const Vector3 waterScale{ gridSize_, gridSize_ * 0.3f, gridSize_ };

    // マップチップの2D配列を取得する
    const auto& mapChips = mapGenerator_->GetMapChips();
    UpdateDistanceMarkers(startX, std::min(endX, mapChips.size()));
    // 描画範囲内のマップチップを描画する
    for (size_t x = startX; x < endX && x < mapChips.size(); ++x) {
        for (size_t z = 0; z < mapChips[x].size(); ++z) {
            const auto chipType = mapGenerator_->GetMapChip(x, z);
            // チップの種類に応じて描画する。
            // 水以外はすべて地面を敷く。空白マスも「何も無い穴」ではなく、
            // 地面の上に壊せない岩を立てた「敷けない床」として見せる。
            if (chipType != MapChipType::Water) {
                // グラウンドチップの表示。マスごとに色をわずかに散らしてマス目を読めるようにする。
                const Vector3 groundPosition{ x * gridSize_, groundHeight, z * gridSize_ };
                const Vector3 toCamera = groundPosition - cameraFocusPosition;
                const float cameraDistance = std::sqrt(
                    toCamera.x * toCamera.x + toCamera.y * toCamera.y + toCamera.z * toCamera.z);
                const Vector4 groundTint = CalcGroundTint(x, z, cameraDistance);
                groundRenderPool_->Draw(groundPosition, rotate, scale, groundTint);
                // 柱は1本に見せたいので、ブロックと同じ色ムラを掛ける
                if (drawGroundSkirt) {
                    groundSkirtRenderPool_->Draw(
                        groundPosition, groundSkirtRotate, groundSkirtScale, groundTint);
                }
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

            // 空白マスの表示。地面は上で敷いてあるので、その上へ壊せない岩を立てる。
            if (hardRockRenderPool_ && chipType == MapChipType::Void) {
                hardRockRenderPool_->Draw(
                    { x * gridSize_, surfaceHeight, z * gridSize_ }, rotate, scale);
            }

            // バナナの木チップの表示。収穫された直後だけサル側へしなる。
            if (bananaTreeRenderPool_ && chipType == MapChipType::BananaTree) {
                Vector3 treeRotate = rotate;
                Vector3 treeScale = scale;
                ApplyBananaTreeShake(x, z, treeRotate, treeScale);
                bananaTreeRenderPool_->Draw(
                    { x * gridSize_, surfaceHeight, z * gridSize_ },
                    treeRotate,
                    treeScale);
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

CoreEngine::Vector4 GameComponents::MapViewComponent::CalcGroundTint(
    std::size_t x, std::size_t z, float cameraDistance) const {
    // 遠いマスは画面上で数ピクセルまで縮むので、ムラを残すとカメラが動くたびにちらつく。
    // 距離でコントラストを 0 まで落とし、地平線側は元の一色へ戻す。
    float fade = 1.0f;
    if (groundTintFadeRange_ > 0.0f) {
        fade = std::clamp(
            1.0f - (cameraDistance - groundTintFadeStart_) / groundTintFadeRange_,
            0.0f, 1.0f);
    }

    // -1..1 のマス固有の値。これ1つで明度と色味の両方を振る。
    // 毎フレーム同じ値でないと、プールの要素が別のマスへ移った瞬間に色がちらつく。
    const float cell01 = Hash::Cell01(
        static_cast<std::int32_t>(x), static_cast<std::int32_t>(z));
    const float amount = (cell01 * 2.0f - 1.0f) * fade;
    const float luminance = 1.0f + groundTintStrength_ * amount;
    // 明度だけだと白黒のムラに見えるので、青チャンネルだけ逆位相に振って
    // 「明るいマスは色が薄い / 暗いマスは色が濃い」という芝のムラらしさを出す。
    const float blue = luminance * (1.0f + groundTintHueSwing_ * amount);
    return { luminance, luminance, blue, 1.0f };
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

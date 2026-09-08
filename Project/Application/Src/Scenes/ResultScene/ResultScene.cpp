#include "pch.h"
#include "ResultScene.h"

#include "Audio/AudioSystem.h"
#include "Camera/Camera.h"
#include "Camera/CameraManager.h"
#include "Components/GameCore/GameResultData.h"
#include "Components/Result/ResultMonkeyShakeComponent.h"
#include "Components/Result/ResultButtonAnimationComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Scenes/GameScene/SkyFogFeature.h"
#include "Scenes/ResultScene/ResultSceneUi.h"
#include "EngineSystem/EngineSystem.h"
#include "Input/InputManager.h"
#include "Scene/SceneManager.h"
#include "UI/UIImage.h"
#include "UI/UIText.h"
#include "Utility/FrameRate/Time.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace CoreEngine;

namespace
{
constexpr const char* kResultBgmPath = "Sounds/BGM/Result_bgm.mp3";
constexpr const char* kRailBuildSePath = "Application/Assets/Sounds/SE/rail_build.mp3";
constexpr const char* kDecisionSePath = "Sounds/SE/decision.mp3";

CVar<float> ResultMonkeyRingRadius{
    "Result.Monkey.RingRadius",
    4.5f,
    "リザルト画面でサルを配置する円形エリアの半径（サル数が多い場合は自動拡張）",
    CVarRange{ 0.0f, 30.0f } };

CVar<float> ResultCameraOrbitSpeed{
    "Result.Camera.Orbit.RotationSpeed",
    0.32f,
    "リザルトカメラの周回速度（ラジアン/秒）",
    CVarRange{ -2.0f, 2.0f } };

constexpr float kResultMonkeyY = 7.8f;
constexpr float kResultMonkeyMinCenterDistance = 1.9f;
constexpr float kResultMonkeyLayoutEdgePadding = 0.9f;
constexpr int kResultMonkeyRandomPlacementAttempts = 512;

float GetResultMonkeyLayoutRadius(std::size_t monkeyCount)
{
    const float configuredRadius = std::max(0.0f, ResultMonkeyRingRadius.Get());
    const float count = static_cast<float>(std::max<std::size_t>(1, monkeyCount));
    const float autoExpandedRadius = kResultMonkeyLayoutEdgePadding
        + kResultMonkeyMinCenterDistance * 0.75f * std::sqrt(count);
    return std::max(configuredRadius, autoExpandedRadius);
}
}

ResultScene::ResultScene::~ResultScene() = default;

void ResultScene::ResultScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("ResultScene");
    // 結果画面専用の地形を使うため、エンジン標準の床は生成しない。
    SetDefaultGroundEnabled(false);

    // 参照ガイドの「CreateObject + MeshRendererComponent」パターンで、
    // 結果画面専用のモデルをコードから構築する。
    auto* resultGround = CreateObject("Result_ground");
    if (resultGround) {
        resultGround->SetSerializeEnabled(true);
        auto* transform = resultGround->AddComponent<TransformComponent>();
        if (transform) {
            transform->Translate() = { 0.0f, -5.0f, 0.0f };
            transform->Scale() = { 100.0f, 1.0f, 100.0f };
        }
        resultGround->AddComponent<MeshRendererComponent>("result_ground.obj");
        resultGround->SetActive(true);
    }

    const std::size_t monkeyCount = std::max<std::size_t>(
        1,
        GameComponents::GameResultData::GetMonkeyCount());
    const std::uint32_t seed =
        0x9E3779B9u
        ^ static_cast<std::uint32_t>(monkeyCount) * 0x85EBCA6Bu
        ^ GameComponents::GameResultData::GetHorizontalProgressBlocks();
    std::mt19937 placementRandom(seed);
    std::uniform_real_distribution<float> yRotationDistribution(
        0.0f,
        2.0f * std::numbers::pi_v<float>);

    const auto createResultMonkey = [this](
        const std::string& name,
        const Vector3& position,
        std::size_t monkeyIndex,
        float yRotation) {
            auto* monkey = CreateObject(name);
            if (!monkey) {
                return;
            }

            monkey->SetSerializeEnabled(true);
            auto* transform = monkey->AddComponent<TransformComponent>();
            if (transform) {
                transform->Translate() = position;
                transform->Rotate() = { 0.0f, yRotation, 0.0f };
            }
            monkey->AddComponent<MeshRendererComponent>("result_monkey.obj");
            monkey->AddComponent<GameComponents::ResultMonkeyShakeComponent>(monkeyIndex);
            monkey->SetActive(true);
        };

    createResultMonkey(
        "Result_monkey",
        { 0.0f, kResultMonkeyY, 0.0f },
        0,
        yRotationDistribution(placementRandom));

    if (monkeyCount > 1) {
        // サルの足元の大きさを基準に、円形エリア内へ候補位置をランダムに生成する。
        // 中央のサルと、すでに配置したサルとの距離を判定するため、同じ円周に並ばず重ならない。
        const float layoutRadius = GetResultMonkeyLayoutRadius(monkeyCount);
        const float placementRadius = std::max(
            0.0f,
            layoutRadius - kResultMonkeyLayoutEdgePadding);
        const float placementRadiusSquared = placementRadius * placementRadius;
        const float minCenterDistanceSquared =
            kResultMonkeyMinCenterDistance * kResultMonkeyMinCenterDistance;
        std::uniform_real_distribution<float> positionDistribution(
            -placementRadius,
            placementRadius);
        std::uniform_real_distribution<float> angleDistribution(
            0.0f,
            2.0f * std::numbers::pi_v<float>);

        std::vector<Vector3> placedPositions;
        placedPositions.reserve(monkeyCount);

        const auto isPositionAvailable = [&](const Vector3& candidate) {
            const float distanceFromCenterSquared =
                candidate.x * candidate.x + candidate.z * candidate.z;
            if (distanceFromCenterSquared < minCenterDistanceSquared
                || distanceFromCenterSquared > placementRadiusSquared) {
                return false;
            }

            for (const Vector3& placed : placedPositions) {
                const float deltaX = candidate.x - placed.x;
                const float deltaZ = candidate.z - placed.z;
                const float distanceSquared = deltaX * deltaX + deltaZ * deltaZ;
                if (distanceSquared < minCenterDistanceSquared) {
                    return false;
                }
            }
            return true;
        };

        for (std::size_t index = 1; index < monkeyCount; ++index) {
            Vector3 selectedPosition{};
            bool positionFound = false;

            for (int attempt = 0;
                attempt < kResultMonkeyRandomPlacementAttempts && !positionFound;
                ++attempt) {
                const Vector3 candidate{
                    positionDistribution(placementRandom),
                    0.0f,
                    positionDistribution(placementRandom) };
                if (isPositionAvailable(candidate)) {
                    selectedPosition = candidate;
                    positionFound = true;
                }
            }

            // 高密度になった場合は、黄金角の候補も試して配置数を確保する。
            if (!positionFound) {
                const float goldenAngle =
                    std::numbers::pi_v<float> * (3.0f - std::sqrt(5.0f));
                const std::size_t fallbackAttempts = std::max<std::size_t>(
                    512,
                    monkeyCount * 256);
                const float radiusSpan = std::max(
                    0.0f,
                    placementRadius - kResultMonkeyMinCenterDistance);
                const float randomPhase = angleDistribution(placementRandom);

                for (std::size_t attempt = 0;
                    attempt < fallbackAttempts && !positionFound;
                    ++attempt) {
                    const float normalizedAttempt = static_cast<float>(attempt + 1)
                        / static_cast<float>(fallbackAttempts);
                    const float candidateRadius =
                        kResultMonkeyMinCenterDistance
                        + radiusSpan * std::sqrt(normalizedAttempt);
                    const float angle = randomPhase
                        + static_cast<float>(attempt) * goldenAngle;
                    const Vector3 candidate{
                        std::cos(angle) * candidateRadius,
                        0.0f,
                        std::sin(angle) * candidateRadius };
                    if (isPositionAvailable(candidate)) {
                        selectedPosition = candidate;
                        positionFound = true;
                    }
                }
            }

            if (!positionFound) {
                break;
            }

            placedPositions.push_back(selectedPosition);
            createResultMonkey(
                "Result_monkey_" + std::to_string(index + 1),
                {
                    selectedPosition.x,
                    kResultMonkeyY,
                    selectedPosition.z
                },
                index,
                yRotationDistribution(placementRandom));
        }
    }

    // ゲームシーンと同じ雲（高さフォグ）。設定は「ゲーム設定」の Game.Fog.* を共有する。
    AddFeature(GameComponents::CreateSkyFogFeature());

    if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
        resultBgm_ = audioSystem->PlayScoped(
            kResultBgmPath,
            { .bus = AudioBus::BGM, .loop = true, .volume = 1.0f / 3.0f });
    }

    const ResultSceneUi::Elements ui = ResultSceneUi::Build(
        [this](const std::string& text,
            float fontSize,
            UIAnchor anchor,
            const Vector2& position,
            const Vector4& color,
            const std::string& name) -> UIText* {
                return CreateText(text, fontSize, anchor, position, color, name);
        },
        [this](const std::string& texturePath,
            const std::string& name) -> UIImage* {
                auto* image = CreateObject<UIImage>();
                if (!image) {
                    return nullptr;
                }
                image->Initialize(texturePath, name);
                return image;
        });

    retryButton_ = ui.retryButton;
    titleButton_ = ui.titleButton;
    SetSelection(selection_, false);
}

void ResultScene::ResultScene::OnUpdate() {
    auto* inputManager = engine_ ? engine_->GetService<InputManager>() : nullptr;
    if (returnRequested_ || !sceneManager_ || !inputManager) {
        return;
    }
    const auto& input = inputManager->GetQuery();
    if (input.IsActionTriggered(InputAction::UICancel)) {
        returnRequested_ = true;
        sceneManager_->ChangeScene("TitleScene");
        return;
    }

    const bool left = input.IsActionTriggered(InputAction::MoveLeft);
    const bool right = input.IsActionTriggered(InputAction::MoveRight);
    if (left != right) {
        SetSelection(
            selection_ == Selection::Retry ? Selection::Title : Selection::Retry,
            true);

        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            audioSystem->PlayOneShot(
                kRailBuildSePath,
                { .bus = AudioBus::SE });
        }
    }
    if (input.IsActionTriggered(InputAction::UIConfirm)) {
        ConfirmSelection();
    }
}

void ResultScene::ResultScene::OnLateUpdate()
{
    UpdateResultCamera();
}

void ResultScene::ResultScene::UpdateResultCamera()
{
    if (!cameraManager_) {
        return;
    }

    auto* camera = cameraManager_->GetCamera(CameraNames::Game);
    if (!camera) {
        return;
    }

    // 配置エリアの端まで収まるよう、サル数に応じてカメラを後退させる。
    const std::size_t monkeyCount = std::max<std::size_t>(
        1,
        GameComponents::GameResultData::GetMonkeyCount());
    const float radius = GetResultMonkeyLayoutRadius(monkeyCount);
    const float cameraDistance = std::max(18.0f, radius + 14.0f);
    resultCameraOrbitAngle_ += ResultCameraOrbitSpeed.Get()
        * std::max(0.0f, Time::UnscaledDeltaTime());
    resultCameraOrbitAngle_ = std::fmod(
        resultCameraOrbitAngle_,
        2.0f * std::numbers::pi_v<float>);
    const Vector3 focus = { 0.0f, kResultMonkeyY, 0.0f };
    camera->SetTranslate({
        std::sin(resultCameraOrbitAngle_) * cameraDistance,
        kResultMonkeyY + 5.5f,
        -std::cos(resultCameraOrbitAngle_) * cameraDistance });
    camera->LookAt(focus);
    camera->UpdateMatrix();
}

void ResultScene::ResultScene::SetSelection(Selection selection, bool playReaction)
{
    selection_ = selection;

    auto* retryAnimation = retryButton_
        ? retryButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
        : nullptr;
    auto* titleAnimation = titleButton_
        ? titleButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
        : nullptr;

    if (retryAnimation) {
        retryAnimation->SetSelected(selection_ == Selection::Retry);
    }
    if (titleAnimation) {
        titleAnimation->SetSelected(selection_ == Selection::Title);
    }

    if (!playReaction) {
        return;
    }

    auto* selectedAnimation = selection_ == Selection::Retry
        ? retryAnimation
        : titleAnimation;
    if (selectedAnimation) {
        selectedAnimation->PlaySelectionReaction();
    }
}

void ResultScene::ResultScene::ConfirmSelection()
{
    if (returnRequested_ || !sceneManager_) {
        return;
    }

    returnRequested_ = true;

    if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
        audioSystem->PlayOneShot(
            kDecisionSePath,
            { .bus = AudioBus::SE });
    }

    const char* nextScene = selection_ == Selection::Retry ? "GameScene" : "TitleScene";
    auto* selectedAnimation = selection_ == Selection::Retry
        ? (retryButton_
            ? retryButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
            : nullptr)
        : (titleButton_
            ? titleButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
            : nullptr);

    auto* unselectedAnimation = selection_ == Selection::Retry
        ? (titleButton_
            ? titleButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
            : nullptr)
        : (retryButton_
            ? retryButton_->GetComponent<GameComponents::ResultButtonAnimationComponent>()
            : nullptr);

    const auto changeScene = [this, nextScene] {
        if (sceneManager_) {
            sceneManager_->ChangeScene(nextScene);
        }
        };

    if (selectedAnimation) {
        if (unselectedAnimation) {
            unselectedAnimation->PlayUnselectedFade();
        }
        selectedAnimation->PlayConfirmReaction(changeScene);
        return;
    }

    changeScene();
}

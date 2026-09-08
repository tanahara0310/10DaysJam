#include "pch.h"
#include "ResultScene.h"

#include "Audio/AudioSystem.h"
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

using namespace CoreEngine;

namespace
{
constexpr const char* kResultBgmPath = "Sounds/BGM/Result_bgm.mp3";
constexpr const char* kRailBuildSePath = "Application/Assets/Sounds/SE/rail_build.mp3";
constexpr const char* kDecisionSePath = "Sounds/SE/decision.mp3";
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

    auto* resultMonkey = CreateObject("Result_monkey");
    if (resultMonkey) {
        resultMonkey->SetSerializeEnabled(true);
        auto* transform = resultMonkey->AddComponent<TransformComponent>();
        if (transform) {
            transform->Translate() = { 0.0f,7.8f, 0.0f };
        }
        resultMonkey->AddComponent<MeshRendererComponent>("result_monkey.obj");
        resultMonkey->SetActive(true);
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

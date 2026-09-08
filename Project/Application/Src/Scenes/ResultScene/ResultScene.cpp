#include "pch.h"
#include "ResultScene.h"

#include "Audio/AudioSystem.h"
#include "Components/GameCore/GameResultData.h"
#include "Components/Result/ResultButtonAnimationComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Scenes/GameScene/SkyFogFeature.h"
#include "Scenes/ResultScene/ResultCameraFeature.h"
#include "Scenes/ResultScene/ResultSceneUi.h"
#include "EngineSystem/EngineSystem.h"
#include "Input/InputManager.h"
#include "Scene/SceneManager.h"
#include "UI/UIText.h"
#include "Utility/Tween/Tween.h"

#include <string>

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
    selection_ = Selection::Retry;
    returnRequested_ = false;
    menuEntranceStarted_ = false;
    menuReady_ = false;
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

    // カメラ入力の後、ライト・影の更新より先にリザルトの構図を確定する。
    AddFeature(GameComponents::CreateResultCameraFeature(), kEarlyFeaturePriority + 1);

    // ゲームシーンと同じ雲（高さフォグ）。設定は「ゲーム設定」の Game.Fog.* を共有する。
    AddFeature(GameComponents::CreateSkyFogFeature());

    if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
        resultBgm_ = audioSystem->PlayScoped(
            kResultBgmPath,
            { .bus = AudioBus::BGM, .loop = true, .volume = 1.0f / 3.0f });
    }

    resultScore_ = GameComponents::GameResultData::GetHorizontalProgressBlocks();
    isNewHighScore_ = GameComponents::GameResultData::SubmitScore(resultScore_);

    const ResultSceneUi::Elements ui = ResultSceneUi::Build(
        [this](const std::string& text,
            float fontSize,
            UIAnchor anchor,
            const Vector2& position,
            const Vector4& color,
            const std::string& name) -> UIText* {
                return CreateText(text, fontSize, anchor, position, color, name);
        });

    scoreText_ = ui.scoreText;
    highScoreText_ = ui.highScoreText;
    recordUpdatedText_ = ui.recordUpdatedText;
    retryButton_ = ui.retryButton;
    titleButton_ = ui.titleButton;
    SetSelection(selection_, false);
    StartScorePresentation();
}

void ResultScene::ResultScene::StartScorePresentation()
{
    if (!scoreText_ || resultScore_ == 0 || ResultSceneUi::ScoreCountUpDuration.Get() <= 0.0f) {
        CompleteScorePresentation();
        return;
    }

    scoreText_->SetText("スコア: 0");

    Tween::To<float>(
        0.0f,
        static_cast<float>(resultScore_),
        ResultSceneUi::ScoreCountUpDuration.Get(),
        [this](const float& displayedScore) {
            if (scoreText_) {
                const uint32_t score = displayedScore >= static_cast<float>(resultScore_)
                    ? resultScore_ : static_cast<uint32_t>(displayedScore);
                scoreText_->SetText("スコア: " + std::to_string(score));
            }
        })
        .SetEase(EasingUtil::Type::EaseOutCubic)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetLink(scoreText_)
        .SetId("result_score_count_up")
        .OnComplete([this] { CompleteScorePresentation(); });
}

void ResultScene::ResultScene::CompleteScorePresentation()
{
    if (menuEntranceStarted_ || returnRequested_) {
        return;
    }
    menuEntranceStarted_ = true;
    if (scoreText_) {
        scoreText_->SetText("スコア: " + std::to_string(resultScore_));
    }

    ResultSceneUi::Elements elements;
    elements.highScoreText = highScoreText_;
    elements.recordUpdatedText = recordUpdatedText_;
    elements.retryButton = retryButton_;
    elements.titleButton = titleButton_;
    ResultSceneUi::PlayMenuEntrance(elements, isNewHighScore_, [this] {
        menuReady_ = true;
    });
    ShowRecordUpdated();
}

void ResultScene::ResultScene::ShowRecordUpdated()
{
    if (!isNewHighScore_ || !recordUpdatedText_) {
        return;
    }

    const Vector4 startColor = recordUpdatedText_->GetColor();
    const Vector4 endColor = ResultSceneUi::RecordUpdatedColor.Get();

    Tween::To<Vector4>(
        startColor,
        endColor,
        ResultSceneUi::RecordUpdatedFadeDuration.Get(),
        [this](const Vector4& color) {
            if (recordUpdatedText_) {
                recordUpdatedText_->SetColor(color);
            }
        })
        .SetEase(EasingUtil::Type::EaseOutCubic)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetLink(recordUpdatedText_)
        .SetId("result_record_updated_fade");
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

    // 表示前・スライド中の見えない選択肢への入力を防ぐ。
    if (!menuReady_) {
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
    if (!menuReady_ || returnRequested_ || !sceneManager_) {
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

void ResultScene::ResultScene::OnFinalize()
{
    // カウントアップ中にキャンセルしても、完了通知を次のシーンへ持ち越さない。
    Tween::KillById("result_score_count_up");
    Tween::KillById("result_record_updated_fade");
    Tween::KillById("result_menu_slide_in");
}

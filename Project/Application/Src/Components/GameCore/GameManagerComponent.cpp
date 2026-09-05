#include "pch.h"
#include "GameManagerComponent.h"

#include "Components/GameCore/GameResultData.h"
#include "Components/Rail/RailBuilderComponent.h"
#include "Components/Train/TrainMovementComponent.h"
#include "Components/Camera/CameraManagerComponent.h"
#include "Scene/SceneManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

json GameComponents::GameManagerComponent::OnSerialize() const {
    return { { "resultTransitionDelay", defaultChangeDelay_ } };
}

void GameComponents::GameManagerComponent::OnDeserialize(const json& j) {
    defaultChangeDelay_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "resultTransitionDelay", defaultChangeDelay_));
}

#ifdef USE_IMGUI
bool GameComponents::GameManagerComponent::DrawInspector() {
    bool changed = ImGui::DragFloat(
        "リザルト遷移待機時間", &defaultChangeDelay_, 0.05f, 0.0f, 10.0f);
    const char* phaseName = phase_ == Phase::Playing ? "Playing"
        : phase_ == Phase::Ending ? "Ending" : "Transitioning";
    ImGui::TextDisabled("現在フェーズ: %s", phaseName);
    return changed;
}
#endif

void GameComponents::GameManagerComponent::Start() {
    // 終了要求を Start() でリセットしない（オブジェクトの更新順に依存させない）。
    if (!sceneManager_ || !sceneManager_->HasScene("ResultScene")) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "GameManager: SceneManager または ResultScene が未設定です");
        SetEnabled(false);
    }
}

void GameComponents::GameManagerComponent::SetGameplayComponents(
    TrainMovementComponent* train, RailBuilderComponent* builder) {
    train_ = train;
    builder_ = builder;
}

void GameComponents::GameManagerComponent::LateUpdate() {
    if (phase_ == Phase::Ending) {
        UpdateEnding();
    }
}

void GameComponents::GameManagerComponent::RequestGameOver(float changeDelayTime) {
    BeginEnding(false, changeDelayTime);
}

void GameComponents::GameManagerComponent::RequestGameClear(float changeDelayTime) {
    BeginEnding(true, changeDelayTime);
}

void GameComponents::GameManagerComponent::BeginEnding(bool isClear, float changeDelayTime) {
    // 最初の終了理由を保持し、重複通知でタイマーをリセットしない。
    if (phase_ != Phase::Playing) {
        return;
    }

    isGameClear_ = isClear;
    isGameOver_ = !isClear;
    phase_ = Phase::Ending;
    changeDelayTimer_ = changeDelayTime >= 0.0f
        ? changeDelayTime
        : defaultChangeDelay_;

    // GameScene のオブジェクトが破棄される前に、リザルト用の共有データへ確定する。
    GameResultData::SetTravelDistance(train_ ? train_->GetTravelDistance() : 0.0f);

    if (train_) {
        train_->SetEnabled(false);
    }
    if (builder_) {
        builder_->SetEnabled(false);
    }

    if (endingCamera_) {
        endingCamera_->BeginTrainCloseUp();
    }

    Logger::GetInstance().Infof(
        LogCategory::Game,
        "GameManager: {}。カメラ演出後 {:.2f} 秒でリザルトへ",
        isClear ? "ゲームクリア" : "ゲームオーバー", changeDelayTimer_);
}

void GameComponents::GameManagerComponent::UpdateEnding() {
    // カメラがアップになってから待機を開始する。未設定・無効なら演出をスキップ。
    if (endingCamera_ && endingCamera_->IsEnabled() && !endingCamera_->IsTrainCloseUpComplete()) {
        return;
    }
    // ゲームプレイ側のポーズ・スローでも終了処理が止まらない時間を使う。
    changeDelayTimer_ -= std::max(0.0f, Time::UnscaledDeltaTime());
    if (changeDelayTimer_ <= 0.0f) {
        TransitionToResult();
    }
}

void GameComponents::GameManagerComponent::TransitionToResult() {
    if (!sceneManager_ || !sceneManager_->HasScene("ResultScene")) {
        return;
    }

    // SceneManager は次フレームで切り替える。要求後は二度と送らない。
    phase_ = Phase::Transitioning;
    sceneManager_->ChangeScene("ResultScene");
}

#include "pch.h"
#include "ResultScene.h"

#include "EngineSystem/EngineSystem.h"
#include "Input/InputManager.h"
#include "Scene/SceneManager.h"
#include "UI/UIText.h"

using namespace CoreEngine;

ResultScene::ResultScene::~ResultScene() = default;

void ResultScene::ResultScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("ResultScene");
    SetDefaultGroundEnabled(true);
    // ========== オブジェクトの生成 ==========
    auto* title = CreateText("RESULT", 96.0f, UIAnchor::Center,
        { 0.0f, -180.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, "ResultTitle");
    if (title) {
        title->SetPivot({ 0.5f, 0.5f });
    }
    retryText_ = CreateText("リトライ", 48.0f, UIAnchor::Center,
        { 0.0f, -30.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, "ResultRetry");
    titleText_ = CreateText("タイトルへ", 48.0f, UIAnchor::Center,
        { 0.0f, 60.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, "ResultReturnToTitle");
    if (retryText_) {
        retryText_->SetPivot({ 0.5f, 0.5f });
    }
    if (titleText_) {
        titleText_->SetPivot({ 0.5f, 0.5f });
    }
    RefreshSelection();

    auto* hint = CreateText("上下で選択 / 決定で進む / キャンセルでタイトルへ", 28.0f, UIAnchor::Center,
        { 0.0f, 170.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, "ResultHint");
    if (hint) {
        hint->SetPivot({ 0.5f, 0.5f });
    }
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

    const bool up = input.IsActionTriggered(InputAction::MoveForward);
    const bool down = input.IsActionTriggered(InputAction::MoveBack);
    if (up != down) {
        selection_ = selection_ == Selection::Retry ? Selection::Title : Selection::Retry;
        RefreshSelection();
    }
    if (input.IsActionTriggered(InputAction::UIConfirm)) {
        returnRequested_ = true;
        // GameScene は新規生成されるため、列車・レール・資源も初期状態から再開する。
        sceneManager_->ChangeScene(selection_ == Selection::Retry ? "GameScene" : "TitleScene");
    }
}

void ResultScene::ResultScene::RefreshSelection() {
    const Vector4 selectedColor{ 1.0f, 0.8f, 0.2f, 1.0f };
    const Vector4 normalColor{ 0.7f, 0.7f, 0.7f, 1.0f };
    if (retryText_) {
        retryText_->SetText(selection_ == Selection::Retry ? "> リトライ <" : "リトライ");
        retryText_->SetColor(selection_ == Selection::Retry ? selectedColor : normalColor);
    }
    if (titleText_) {
        titleText_->SetText(selection_ == Selection::Title ? "> タイトルへ <" : "タイトルへ");
        titleText_->SetColor(selection_ == Selection::Title ? selectedColor : normalColor);
    }
}

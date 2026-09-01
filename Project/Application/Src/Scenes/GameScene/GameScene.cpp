#include "pch.h"
#include "GameScene.h"

GameScene::GameScene::~GameScene() = default;

void GameScene::GameScene::OnInitialize() {
    SetSceneName("GameScene");
    SetReleaseCameraTransform({ 0.0f, 3.0f, 0.0f });
}

void GameScene::GameScene::OnUpdate() {
}

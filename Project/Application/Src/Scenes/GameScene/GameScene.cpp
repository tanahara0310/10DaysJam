#include "pch.h"
#include "GameScene.h"

#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"

#include "Components/Camera/CameraManagerComponent.h"
#include "Components/Rail/RailBuilderComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/Rail/RailViewComponent.h"
#include "Components/Train/TrainMovementComponent.h"

GameScene::GameScene::~GameScene() = default;

void GameScene::GameScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("GameScene");
    SetDefaultGroundEnabled(true);
    SetReleaseCameraTransform({ 0.0f, 2.0f, 0.0f }, { 0.3f, 0.0f, 0.0f });

    // ========== ゲームルールの設定 ==========
    float gridSize = 5.0f; // グリッドサイズを設定
    uint32_t mapSizeX = 30; // マップのX方向のサイズを設定
    uint32_t mapSizeZ = 10; // マップのZ方向のサイズを設定

    uint32_t initialBuilderPosX = 3; // 初期のビルダーのX座標を設定
    uint32_t initialBuilderPosZ = mapSizeZ / 2; // 初期のビルダーのZ座標を設定

    // ========== オブジェクトの生成 ==========
    // レールの配置を管理するコンポーネントを追加
    auto* railPath = CreateObject("RailPath");
    railPath->AddComponent<CoreEngine::TransformComponent>();
    railPath->AddComponent<GameComponents::RailPathComponent>(
        mapSizeX, mapSizeZ, initialBuilderPosX, initialBuilderPosZ);

    // レールを表示するコンポーネントを追加
    auto* railView = CreateObject("RailView");
    railView->AddComponent<CoreEngine::TransformComponent>();
    railView->AddComponent<GameComponents::RailViewComponent>(
        gridSize, railPath->GetComponent<GameComponents::RailPathComponent>());
    railView;

    // レールを配置するオブジェクトを生成
    auto* railBuilder = CreateObject("RailBuilder");
    railBuilder->AddComponent<CoreEngine::TransformComponent>();
    railBuilder->AddComponent<GameComponents::RailBuilderComponent>(
        gridSize, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>());

    railBuilder->AddComponent< CoreEngine::MeshRendererComponent>("box.obj");

    // 列車の移動ロジックを持つオブジェクト。描画とアニメーションは別コンポーネントで追加する。
    auto* train = CreateObject("Train");
    train->AddComponent<CoreEngine::TransformComponent>();
    train->AddComponent<GameComponents::TrainMovementComponent>(
        gridSize, 0.5f, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>());

    train->AddComponent< CoreEngine::MeshRendererComponent>("box.obj");

    // 列車とビルダーの中間を捉え、距離に応じて視野角を変えるゲームカメラ。
    auto* cameraController = CreateObject("CameraManager");
    cameraController->AddComponent<GameComponents::CameraManagerComponent>(
        cameraManager_->GetCamera(CoreEngine::CameraNames::Game),
        train->GetComponent<CoreEngine::TransformComponent>(),
        railBuilder->GetComponent<CoreEngine::TransformComponent>(),
        0.4f); // カメラX位置: 0.0 = 列車側、0.5 = 中間、1.0 = ビルダー側
}

void GameScene::GameScene::OnUpdate() {
}

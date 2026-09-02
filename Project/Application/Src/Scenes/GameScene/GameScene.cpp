#include "pch.h"
#include "GameScene.h"

#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "EngineSystem/EngineSystem.h"
#include "Text/FontManager.h"
#include "UI/UIText.h"
#include "Utility/Logger/Logger.h"

#include "Components/Utility/ModelRenderPoolComponent.h"

#include "Components/Building/MapGeneratorComponent.h"
#include "Components/Building/MapViewComponent.h"
#include "Components/Camera/CameraManagerComponent.h"
#include "Components/Rail/RailBuilderComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/Rail/RailViewComponent.h"
#include "Components/Rail/RailResourceManagerComponent.h"
#include "Components/Train/TrainMovementComponent.h"
#include "Components/UI/RailResourceUIComponent.h"

GameScene::GameScene::~GameScene() = default;

void GameScene::GameScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("GameScene");
    SetDefaultGroundEnabled(true);
    SetReleaseCameraTransform({ 0.0f, 2.0f, 0.0f }, { 0.3f, 0.0f, 0.0f });

    // ========== ゲームルールの設定 ==========
    float gridSize = 1.0f; // グリッドサイズを設定
    uint32_t mapSizeZ = 9; // マップのZ方向のサイズを設定

    uint32_t railResourceCount = 15; // 初期のレールリソース数を設定
    uint32_t initialBuilderPosX = 3; // 初期のビルダーのX座標を設定
    uint32_t initialBuilderPosZ = mapSizeZ / 2; // 初期のビルダーのZ座標を設定

    uint32_t initialGenerateMapSizeX = 30; // 初期生成マップのX方向のサイズを設定
    uint32_t renderWorldDistance = 20; // 描画するワールドの距離を設定

    // ========== オブジェクトの生成 ==========
    // 床のオブジェクトプールを生成
    auto* groundPoolManager = CreateObject("GroundPoolManager");
    groundPoolManager->AddComponent<CoreEngine::TransformComponent>();
    groundPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "box.obj", 600,false);
    // 水場のオブジェクトプールを生成
    auto* waterPoolManager = CreateObject("WaterPoolManager");
    waterPoolManager->AddComponent<CoreEngine::TransformComponent>();
    waterPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "box.obj", 100, false,
        CoreEngine::Vector4{ 0.0f, 0.35f, 0.65f, 1.0f });
    // 駅のオブジェクトプールを生成
    auto* stationPoolManager = CreateObject("StationPoolManager");
    stationPoolManager->AddComponent<CoreEngine::TransformComponent>();
    stationPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "station.obj", 10, false);
    // 岩のオブジェクトプールを生成
    auto* rockPoolManager = CreateObject("RockPoolManager");
    rockPoolManager->AddComponent<CoreEngine::TransformComponent>();
    rockPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rock.obj", 50, false);
    // レールのオブジェクトプールを生成
    auto* railPoolManager = CreateObject("RailPoolManager");
    railPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail.obj", 100, false);
    // レール左のオブジェクトプールを生成
    auto* railLeftPoolManager = CreateObject("RailLeftPoolManager");
    railLeftPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railLeftPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail_l.obj", 50, false);
    // レール右のオブジェクトプールを生成
    auto* railRightPoolManager = CreateObject("RailRightPoolManager");
    railRightPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railRightPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail_r.obj", 50, false);

    // マップを生成するコンポーネントを追加
    auto* mapGenerator = CreateObject("MapGenerator");
    mapGenerator->AddComponent<CoreEngine::TransformComponent>();
    mapGenerator->AddComponent<GameComponents::MapGeneratorComponent>(
        mapSizeZ, initialGenerateMapSizeX);
    
    // レールの配置を管理するコンポーネントを追加
    auto* railPath = CreateObject("RailPath");
    railPath->AddComponent<CoreEngine::TransformComponent>();
    railPath->AddComponent<GameComponents::RailPathComponent>(
        mapSizeZ, initialBuilderPosX, initialBuilderPosZ);

    // レールを表示するコンポーネントを追加
    auto* railView = CreateObject("RailView");
    railView->AddComponent<CoreEngine::TransformComponent>();

    // レールを配置するオブジェクトを生成
    auto* railBuilder = CreateObject("RailBuilder");
    railBuilder->AddComponent<CoreEngine::TransformComponent>();
    railBuilder->AddComponent<GameComponents::RailResourceManagerComponent>(railResourceCount);

    // 列車の移動ロジックを持つオブジェクト。描画とアニメーションは別コンポーネントで追加する。
    auto* train = CreateObject("Train");
    train->AddComponent<CoreEngine::TransformComponent>();
    train->AddComponent<GameComponents::TrainMovementComponent>(
        gridSize, 0.5f, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>());

    train->AddComponent< CoreEngine::MeshRendererComponent>("trolley.obj");

    railBuilder->AddComponent<GameComponents::RailBuilderComponent>(
        gridSize, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        railBuilder->GetComponent<GameComponents::RailResourceManagerComponent>(),
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        train->GetComponent<GameComponents::TrainMovementComponent>());

    railBuilder->AddComponent<CoreEngine::MeshRendererComponent>("monkey.obj");

    // 列車とビルダーの中間を捉え、距離に応じて視野角を変えるゲームカメラ。
    auto* cameraController = CreateObject("CameraManager");
    cameraController->AddComponent<GameComponents::CameraManagerComponent>(
        cameraManager_->GetCamera(CoreEngine::CameraNames::Game),
        train->GetComponent<CoreEngine::TransformComponent>(),
        railBuilder->GetComponent<CoreEngine::TransformComponent>(),
        0.4f); // カメラX位置: 0.0 = 列車側、0.5 = 中間、1.0 = ビルダー側

    railView->AddComponent<GameComponents::RailViewComponent>(
        gridSize,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        railPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railLeftPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railRightPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        cameraController->GetComponent<GameComponents::CameraManagerComponent>(),
        renderWorldDistance);

    // マップを描画するオブジェクトを追加
    auto* mapRenderer = CreateObject("MapRenderer");
    mapRenderer->AddComponent<CoreEngine::TransformComponent>();
    mapRenderer->AddComponent<GameComponents::MapViewComponent>(
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        groundPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        waterPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        stationPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        rockPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        cameraController->GetComponent<GameComponents::CameraManagerComponent>(),
        gridSize, renderWorldDistance);

    // 残りレール数を画面左上へ表示するHUD
    auto* fontManager = engine_->GetService<CoreEngine::FontManager>();
    if (fontManager) {
        CoreEngine::MsdfFontDesc fontDesc;
        fontDesc.filePath = L"Engine/Assets/font/851Gkktt_005.ttf";
        fontDesc.systemFamilyNames = {
            L"Yu Gothic UI", L"Meiryo", L"Segoe UI"
        };
        fontDesc.charsetUtf8 = "残りレール: 0123456789";

        if (auto* font = fontManager->Acquire(fontDesc)) {
            auto* railResourceText = CreateObject<CoreEngine::UIText>();
            railResourceText->Initialize(font, "残りレール: 0", "RailResourceText");
            railResourceText->SetAnchor(CoreEngine::UIAnchor::TopLeft);
            railResourceText->SetAnchoredPosition({ 32.0f, 32.0f });
            railResourceText->SetPivot({ 0.0f, 0.0f });
            railResourceText->SetFontSize(36.0f*3.0f);
            railResourceText->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            railResourceText->SetOutline({ 0.0f, 0.0f, 0.0f, 1.0f }, 0.035f);
            railResourceText->SetSortOrder(1000);
            railResourceText->AddComponent<GameComponents::RailResourceUIComponent>(
                railBuilder->GetComponent<GameComponents::RailResourceManagerComponent>());
        }
    } else {
        CoreEngine::Logger::GetInstance().Errorf(
            CoreEngine::LogCategory::Game,
            "GameScene: FontManager が取得できないためレール数UIを生成できません");
    }
}

void GameScene::GameScene::OnUpdate() {
}

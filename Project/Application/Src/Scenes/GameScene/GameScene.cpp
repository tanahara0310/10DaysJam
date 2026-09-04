#include "pch.h"
#include "GameScene.h"

#include "EngineSystem/EngineSystem.h"
#include "Audio/AudioSystem.h"
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

#include "Components/GameCore/GameManagerComponent.h"
#include "Components/GameCore/GameSettingsComponent.h"
#include "GameObjects/GameSceneObject.h"

#include <algorithm>

using namespace CoreEngine;

namespace {
    constexpr const char* kGameBgmPath = "Application/Assets/Sounds/BGM/Game_bgm.mp3";

    uint32_t ToUInt(int value, int minimum = 0) {
        return static_cast<uint32_t>(std::max(value, minimum));
    }
}

GameScene::GameScene::~GameScene() = default;

void GameScene::GameScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("GameScene");
    SetDefaultGroundEnabled(true);
    SetReleaseCameraTransform(
        GameComponents::GameSettings::ReleaseCameraPosition.Get(),
        GameComponents::GameSettings::ReleaseCameraRotation.Get());

    // ========== BGMの再生 ==========
    auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr;
    if (!audioSystem) {
        return;
    }
    gameBgm_ = audioSystem->PlayScoped(
        kGameBgmPath,
        { .bus = AudioBus::BGM, .loop = true,
          .volume = GameComponents::GameSettings::BgmVolume.Get() });

    // ========== SEの登録 ==========
    std::function<void()> playDecisionSe = [this] {
        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            audioSystem->PlayOneShot(
                "Application/Assets/Sounds/SE/decision.mp3",
                { .bus = AudioBus::SE });
        }
        };
    std::function<void()> playBuildSe = [this] {
        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            int randomIndex = rand() % 100;
            float pitch = 1.0f;
            if (randomIndex < 20) {
                pitch = 0.8f; // 20%の確率でピッチを下げる
            } else if (randomIndex < 40) {
                pitch = 1.2f; // 次の20%の確率でピッチを上げる
            }

            audioSystem->PlayOneShot(
                "Application/Assets/Sounds/SE/build.mp3",
                { .bus = AudioBus::SE,.volume = 0.5f, .pitch = pitch });
        }
        };
    std::function<void()> playUndoSe = [this] {
        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            audioSystem->PlayOneShot(
                "Application/Assets/Sounds/SE/build_return.mp3",
                { .bus = AudioBus::SE });
        }
        };
    std::function<void(float, float)> playRailBuildSe = [this](float volume, float pitch) {
        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            CoreEngine::PlayParams params;
            params.bus = AudioBus::SE;
            params.volume = volume;
            params.pitch = pitch;
            audioSystem->PlayOneShot(
                "Application/Assets/Sounds/SE/rail_build.mp3", params);
        }
        };

    // ========== ゲームルールの設定 ==========
    const float gridSize = GameComponents::GameSettings::GridSize.Get();
    const uint32_t mapSizeZ = ToUInt(
        GameComponents::GameSettings::MapSizeZ.Get(), 1);

    const uint32_t railResourceCount = ToUInt(
        GameComponents::GameSettings::InitialRailResources.Get());
    const uint32_t initialBuilderPosX = ToUInt(
        GameComponents::GameSettings::BuilderStartX.Get());
    const uint32_t initialBuilderPosZ = std::min(
        ToUInt(GameComponents::GameSettings::BuilderStartZ.Get()),
        mapSizeZ - 1);

    const uint32_t initialGenerateMapSizeX = ToUInt(
        GameComponents::GameSettings::InitialMapSizeX.Get(), 1);
    const uint32_t renderWorldDistance = ToUInt(
        GameComponents::GameSettings::RenderDistance.Get(), 1);

    // FixedCsv にすると fixedCsvPath の1枚を使用し、終端以降はVoidになる。
    // Procedural にすると従来のチップ単位のランダム生成を使用する。
    GameComponents::MapGenerationSettings mapSettings;
    mapSettings.mode = GameComponents::MapGenerationMode::RandomCsvPool;
    mapSettings.csvChunkSizeX = ToUInt(
        GameComponents::GameSettings::CsvChunkSizeX.Get(), 1);
    // 1プール = 1エリアで使用する複数の区画CSV。地形の種類では分けない。
    // Area1中はArea1内だけ、Area2へ切替後はArea2内だけから区画を抽選する。
    mapSettings.csvPools = {
        { "Area1", {
            "Application/Assets/Maps/Areas/Area1/chunk_01.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_02.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_03.csv",
        } },
        { "Area2", {
            "Application/Assets/Maps/Areas/Area2/chunk_01.csv",
            "Application/Assets/Maps/Areas/Area2/chunk_02.csv",
            "Application/Assets/Maps/Areas/Area2/chunk_03.csv",
        } },
    };
    mapSettings.initialCsvPoolName = "Area1";
    mapSettings.fixedCsvPath = "Application/Assets/Maps/fixed.csv";

    // ========== オブジェクトの生成 ==========
    //　ゲームマスターの追加
    auto* gameManager = CreateObject<GameSceneObject>("GameManager");
    auto* gameManagerComponent =
        gameManager->AddComponent<GameComponents::GameManagerComponent>(sceneManager_);
    gameManager->AddComponent<GameComponents::GameSettingsComponent>();

    // 床のオブジェクトプールを生成
    auto* groundPoolManager = CreateObject<GameSceneObject>("GroundPoolManager");
    groundPoolManager->AddComponent<CoreEngine::TransformComponent>();
    groundPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "ground.obj",
        ToUInt(GameComponents::GameSettings::GroundPoolCapacity.Get(), 1), false);
    // 水場のオブジェクトプールを生成
    auto* waterPoolManager = CreateObject<GameSceneObject>("WaterPoolManager");
    waterPoolManager->AddComponent<CoreEngine::TransformComponent>();
    waterPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "box.obj",
        ToUInt(GameComponents::GameSettings::WaterPoolCapacity.Get(), 1), true,
        CoreEngine::Vector4{ 0.0f, 0.35f, 0.65f, 1.0f });
    // 駅のオブジェクトプールを生成
    auto* stationPoolManager = CreateObject<GameSceneObject>("StationPoolManager");
    stationPoolManager->AddComponent<CoreEngine::TransformComponent>();
    stationPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "station.obj",
        ToUInt(GameComponents::GameSettings::StationPoolCapacity.Get(), 1), true);
    // 岩のオブジェクトプールを生成
    auto* rockPoolManager = CreateObject<GameSceneObject>("RockPoolManager");
    rockPoolManager->AddComponent<CoreEngine::TransformComponent>();
    rockPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rock.obj",
        ToUInt(GameComponents::GameSettings::RockPoolCapacity.Get(), 1), true);
    // レールのオブジェクトプールを生成
    auto* railPoolManager = CreateObject<GameSceneObject>("RailPoolManager");
    railPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail.obj",
        ToUInt(GameComponents::GameSettings::RailPoolCapacity.Get(), 1), false);
    // レール左のオブジェクトプールを生成
    auto* railLeftPoolManager = CreateObject<GameSceneObject>("RailLeftPoolManager");
    railLeftPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railLeftPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail_l.obj",
        ToUInt(GameComponents::GameSettings::RailLeftPoolCapacity.Get(), 1), false);
    // レール右のオブジェクトプールを生成
    auto* railRightPoolManager = CreateObject<GameSceneObject>("RailRightPoolManager");
    railRightPoolManager->AddComponent<CoreEngine::TransformComponent>();
    railRightPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "rail_r.obj",
        ToUInt(GameComponents::GameSettings::RailRightPoolCapacity.Get(), 1), false);

    // マップを生成するコンポーネントを追加
    auto* mapGenerator = CreateObject<GameSceneObject>("MapGenerator");
    mapGenerator->AddComponent<CoreEngine::TransformComponent>();
    mapGenerator->AddComponent<GameComponents::MapGeneratorComponent>(
        mapSizeZ, initialGenerateMapSizeX, mapSettings);
    
    // レールの配置を管理するコンポーネントを追加
    auto* railPath = CreateObject<GameSceneObject>("RailPath");
    railPath->AddComponent<CoreEngine::TransformComponent>();
    railPath->AddComponent<GameComponents::RailPathComponent>(
        mapSizeZ, initialBuilderPosX, initialBuilderPosZ);

    // レールを表示するコンポーネントを追加
    auto* railView = CreateObject<GameSceneObject>("RailView");
    railView->AddComponent<CoreEngine::TransformComponent>();

    // レールを配置するオブジェクトを生成
    auto* railBuilder = CreateObject<GameSceneObject>("RailBuilder");
    railBuilder->AddComponent<CoreEngine::TransformComponent>();
    railBuilder->AddComponent<GameComponents::RailResourceManagerComponent>(railResourceCount);

    // 列車の移動ロジックを持つオブジェクト。描画とアニメーションは別コンポーネントで追加する。
    auto* train = CreateObject<GameSceneObject>("Train");
    auto* trainTransform = train->AddComponent<CoreEngine::TransformComponent>();
    train->AddComponent<GameComponents::TrainMovementComponent>(
        gridSize, GameComponents::GameSettings::TrainMoveSpeed.Get(),
        initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        gameManagerComponent);

    train->AddComponent< CoreEngine::MeshRendererComponent>("trolley.obj");
    // 列車の描画は、列車の移動ロジックを持つコンポーネントとは別のコンポーネントで行う。
    railBuilder->AddComponent<GameComponents::RailBuilderComponent>(
        gridSize, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        railBuilder->GetComponent<GameComponents::RailResourceManagerComponent>(),
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        train->GetComponent<GameComponents::TrainMovementComponent>(),
        playBuildSe, playUndoSe);

    railBuilder->AddComponent<CoreEngine::MeshRendererComponent>("arrow.obj");

    gameManagerComponent->SetGameplayComponents(
        train->GetComponent<GameComponents::TrainMovementComponent>(),
        railBuilder->GetComponent<GameComponents::RailBuilderComponent>());

    // 列車に乗るサル
    auto* monkey = CreateObject<GameSceneObject>("Monkey");
    auto* monkeyTransform = monkey->AddComponent<CoreEngine::TransformComponent>();
    monkey->AddComponent<CoreEngine::MeshRendererComponent>("monkey.obj");
    monkeyTransform->Get().SetParent(&trainTransform->Get());
    monkeyTransform->Get().rotate.y = 3.14f;

    // 列車とビルダーの中間を捉え、距離に応じて視野角を変えるゲームカメラ。
    auto* cameraController = CreateObject<GameSceneObject>("CameraManager");
    cameraController->AddComponent<GameComponents::CameraManagerComponent>(
        cameraManager_->GetCamera(CoreEngine::CameraNames::Game),
        train->GetComponent<CoreEngine::TransformComponent>(),
        railBuilder->GetComponent<CoreEngine::TransformComponent>(),
        GameComponents::GameSettings::CameraFocusRatio.Get(),
        GameComponents::GameSettings::CameraOffset.Get(),
        GameComponents::GameSettings::CameraMinTargetDistance.Get(),
        GameComponents::GameSettings::CameraMaxTargetDistance.Get(),
        GameComponents::GameSettings::CameraMinFovDegrees.Get(),
        GameComponents::GameSettings::CameraMaxFovDegrees.Get(),
        GameComponents::GameSettings::CameraFollowSpeed.Get());

    gameManagerComponent->SetEndingCamera(
        cameraController->GetComponent<GameComponents::CameraManagerComponent>());

    railView->AddComponent<GameComponents::RailViewComponent>(
        gridSize,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        railPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railLeftPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railRightPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        cameraController->GetComponent<GameComponents::CameraManagerComponent>(),
        playRailBuildSe,
        renderWorldDistance);

    // マップを描画するオブジェクトを追加
    auto* mapRenderer = CreateObject<GameSceneObject>("MapRenderer");
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
            railResourceText->SetAnchoredPosition(
                GameComponents::GameSettings::HudPosition.Get());
            railResourceText->SetPivot({ 0.0f, 0.0f });
            railResourceText->SetFontSize(
                GameComponents::GameSettings::HudFontSize.Get());
            railResourceText->SetColor(
                GameComponents::GameSettings::HudColor.Get());
            railResourceText->SetOutline(
                GameComponents::GameSettings::HudOutlineColor.Get(),
                GameComponents::GameSettings::HudOutlineWidth.Get());
            railResourceText->SetSortOrder(
                GameComponents::GameSettings::HudSortOrder.Get());
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

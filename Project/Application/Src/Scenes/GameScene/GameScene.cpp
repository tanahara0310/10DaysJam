#include "pch.h"
#include "GameScene.h"

#include "EngineSystem/EngineSystem.h"
#include "Audio/AudioSystem.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "EngineSystem/EngineSystem.h"
#include "Scene/Feature/TimeOfDayFeature.h"
#include "SkyFogFeature.h"
#include "SpeedGaugeFeature.h"
#include "StageLightsFeature.h"
#include "StaminaGaugeFeature.h"
#include "Utility/Logger/Logger.h"

#include "Components/Utility/ModelRenderPoolComponent.h"

#include "Components/Building/MapGeneratorComponent.h"
#include "Components/Building/MapViewComponent.h"
#include "Components/Building/RockThrowComponent.h"
#include "Components/Camera/RockBreakShakeSettingsComponent.h"
#include "Components/Rail/RailBuilderComponent.h"
#include "Components/Rail/RailPathComponent.h"
#include "Components/Rail/RailViewComponent.h"
#include "Components/Train/SpawnPopComponent.h"
#include "Components/Train/TrainMovementComponent.h"
#include "Components/UI/GameStartPromptAnimationComponent.h"

#include "Components/GameCore/GameManagerComponent.h"
#include "Components/GameCore/GameResultData.h"
#include "Components/GameCore/GameSettingsComponent.h"
#include "Components/GameCore/HungerComponent.h"
#include "GameObjects/Effect/RockBreakDebris.h"
#include "GameObjects/GameSceneObject.h"
#include "UI/UIText.h"

#include <algorithm>
#include <string>

using namespace CoreEngine;

namespace {
    constexpr const char* kGameBgmPath = "Application/Assets/Sounds/BGM/Game_bgm.mp3";

    uint32_t ToUInt(int value, int minimum = 0) {
        return static_cast<uint32_t>(std::max(value, minimum));
    }
}

GameScene::GameScene::~GameScene() = default;

void GameScene::GameScene::OnInitialize() {
    GameComponents::GameResultData::Reset();

    // ========== シーンの設定 ==========
    SetSceneName("GameScene");
    SetDefaultGroundEnabled(true);

    // ゲーム開始時の目標距離を、右から中央へ入り、2秒滞在してから
    // 左へ抜ける案内として表示する。
    auto* startPrompt = CreateText(
        "200ｍすすめ！",
        72.0f,
        UIAnchor::Center,
        { 0.0f, 0.0f },
        { 1.0f, 0.92f, 0.58f, 1.0f },
        "GameStartDistancePrompt");
    if (startPrompt) {
        startPrompt->SetSerializeEnabled(false);
        startPrompt->SetPivot({ 0.5f, 0.5f });
        startPrompt->SetOutline({ 0.04f, 0.02f, 0.0f, 1.0f }, 0.045f);
        startPrompt->SetSortOrder(1000);
        startPrompt->AddComponent<GameComponents::GameStartPromptAnimationComponent>();
    }

    // ========== 昼夜サイクル ==========
    // 時刻を進めて空と太陽・月を昼→夕→夜と変えるだけの Feature。
    // 進み方（1 周の秒数・開始時刻）は Engine Settings の "Time of Day" から調整する。
    AddFeature(std::make_unique<CoreEngine::TimeOfDayFeature>());
    // 夕方から夜にかけて灯る、ビルダーとトロッコの灯り（ポイントライト）
    AddFeature(std::make_unique<StageLightsFeature>());
    // ステージのブロックより下を埋める雲（高さフォグ）。
    // 濃さ・色・高さは「ゲーム設定」の Game.Fog.* から調整する。
    AddFeature(GameComponents::CreateSkyFogFeature());
    // スタミナをバナナの粒で見せる HUD ゲージ。
    // 位置・粒あたりのスタミナ量は「ゲーム設定」の Game.StaminaGauge.* から調整する。
    AddFeature(GameComponents::CreateStaminaGaugeFeature());
    // トロッコの速さを km/h のオドメーターで見せる HUD。
    // 位置・1 マスの実距離は「ゲーム設定」の Game.SpeedGauge.* から調整する。
    AddFeature(GameComponents::CreateSpeedGaugeFeature());

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
            int randomIndex = rand() % 50;
            float pitch = 0.5f + (static_cast<float>(randomIndex) / 50.0f);

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
    std::function<void()> playFailureSe = [this] {
        if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
            audioSystem->PlayOneShot(
                "Application/Assets/Sounds/SE/beep.mp3",
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
    // Area内の全区画をランダム順で一巡し、使い切ったら再シャッフルする。
    mapSettings.csvPools = {
        { "Area1", {
            "Application/Assets/Maps/Areas/Area1/chunk_01.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_02.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_03.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_04.csv",
            "Application/Assets/Maps/Areas/Area1/chunk_05.csv",
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
    // バナナの木のオブジェクトプールを生成（仮モデルとしてbox.objを使用）
    auto* bananaTreePoolManager = CreateObject<GameSceneObject>("BananaTreePoolManager");
    bananaTreePoolManager->AddComponent<CoreEngine::TransformComponent>();
    bananaTreePoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "banana_tree.obj",
        ToUInt(GameComponents::GameSettings::BananaTreePoolCapacity.Get(), 1), true);
    // 地面の上に表示する装飾用の草のオブジェクトプールを生成
    auto* grassPoolManager = CreateObject<GameSceneObject>("GrassPoolManager");
    grassPoolManager->AddComponent<CoreEngine::TransformComponent>();
    grassPoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "grass.obj",
        ToUInt(GameComponents::GameSettings::GrassPoolCapacity.Get(), 1), true);
    // 水上レールの下へ表示する橋のオブジェクトプールを生成
    auto* bridgePoolManager = CreateObject<GameSceneObject>("BridgePoolManager");
    bridgePoolManager->AddComponent<CoreEngine::TransformComponent>();
    bridgePoolManager->AddComponent<GameComponents::ModelRenderPoolComponent>(
        "bridge.obj",
        ToUInt(GameComponents::GameSettings::BridgePoolCapacity.Get(), 1), true);
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

    // 建設行動で消費し、バナナの木で回復するスタミナを管理する。
    auto* hungerComponent = gameManager->AddComponent<GameComponents::HungerComponent>(
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        gameManagerComponent);
    
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

    // 列車の移動ロジックを持つオブジェクト。描画とアニメーションは別コンポーネントで追加する。
    auto* train = CreateObject<GameSceneObject>("Train");
    auto* trainTransform = train->AddComponent<CoreEngine::TransformComponent>();
    auto* trainMovement = train->AddComponent<GameComponents::TrainMovementComponent>(
        gridSize, GameComponents::GameSettings::TrainMoveSpeed.Get(),
        initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        gameManagerComponent,
        hungerComponent);

    train->AddComponent< CoreEngine::MeshRendererComponent>("trolley.obj");

    // 岩破壊時に列車から投げる石。アニメーションはゲーム終了演出中も完了させる。
    auto* rockProjectile = CreateObject<GameSceneObject>("RockProjectile");
    rockProjectile->AddComponent<CoreEngine::TransformComponent>();
    rockProjectile->AddComponent<CoreEngine::MeshRendererComponent>("rock.obj");
    auto* rockThrow = rockProjectile->AddComponent<GameComponents::RockThrowComponent>();

    // 列車の描画は、列車の移動ロジックを持つコンポーネントとは別のコンポーネントで行う。
    railBuilder->AddComponent<GameComponents::RailBuilderComponent>(
        gridSize, initialBuilderPosX, initialBuilderPosZ,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        train->GetComponent<GameComponents::TrainMovementComponent>(),
        hungerComponent,
        rockThrow,
        playBuildSe, playUndoSe, playFailureSe);

    railBuilder->AddComponent<CoreEngine::MeshRendererComponent>("arrow.obj");

    gameManagerComponent->SetGameplayComponents(
        train->GetComponent<GameComponents::TrainMovementComponent>(),
        railBuilder->GetComponent<GameComponents::RailBuilderComponent>(),
        hungerComponent);

    // 列車に乗るサル
    auto* monkey = CreateObject<GameSceneObject>("Monkey");
    auto* monkeyTransform = monkey->AddComponent<CoreEngine::TransformComponent>();
    monkey->AddComponent<CoreEngine::MeshRendererComponent>("monkey.obj");
    monkeyTransform->Get().SetParent(&trainTransform->Get());
    monkeyTransform->Get().rotate.y = 3.14f;
    trainMovement->AddMonkey(monkeyTransform);
    hungerComponent->SetMonkeyAddedCallback(
        [this, trainMovement, monkeyTransform](std::size_t monkeyCount) {
            auto* carriage = CreateObject<GameSceneObject>(
                "TrainCarriage_" + std::to_string(monkeyCount));
            if (!carriage) {
                return;
            }
            auto* carriageTransform = carriage->AddComponent<CoreEngine::TransformComponent>();
            carriage->AddComponent<CoreEngine::MeshRendererComponent>("trolley.obj");
            // 連結より前に付けて、最初のスケール計算から出現演出を効かせる。
            // 子のサルは親のスケールを継ぐので、まとめて潰れる。
            auto* carriagePop =
                carriage->AddComponent<GameComponents::SpawnPopComponent>();
            trainMovement->AddCarriage(
                carriageTransform, carriagePop->GetScaleMultiplier());

            auto* addedMonkey = CreateObject<GameSceneObject>(
                "Monkey_" + std::to_string(monkeyCount));
            if (!addedMonkey) {
                return;
            }
            auto* addedTransform = addedMonkey->AddComponent<CoreEngine::TransformComponent>();
            addedMonkey->AddComponent<CoreEngine::MeshRendererComponent>("monkey.obj");
            if (addedTransform) {
                addedTransform->Get().SetParent(&carriageTransform->Get());
                addedTransform->Get().translate = monkeyTransform->Get().translate;
                addedTransform->Get().rotate = monkeyTransform->Get().rotate;
                addedTransform->Get().scale = monkeyTransform->Get().scale;
                trainMovement->AddMonkey(addedTransform);
            }
        });

    // カメラの構図は Presets/CameraRigs/GamePlay.json が持つ。
    // 起動は _camera.json の startupRigName 任せで、ここでは何も駆動しない。
    auto* gameCamera = cameraManager_->GetCamera(CoreEngine::CameraNames::Game);

    // 岩破壊の揺れは静的に鳴らす。ここでは調整用CVarをインスペクタへ出すために付ける。
    auto* cameraSettings = CreateObject<GameSceneObject>("CameraSettings");
    cameraSettings->AddComponent<GameComponents::RockBreakShakeSettingsComponent>();

    // 岩が砕けた瞬間に散る破片。揺れと同じく RailBuilder から静的に鳴らす。
    AddFeature(GameComponents::CreateRockBreakDebrisFeature());

    railView->AddComponent<GameComponents::RailViewComponent>(
        gridSize,
        railPath->GetComponent<GameComponents::RailPathComponent>(),
        railPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railLeftPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        railRightPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        bridgePoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        gameCamera,
        playRailBuildSe,
        renderWorldDistance);

    // マップを描画するオブジェクトを追加
    auto* mapRenderer = CreateObject<GameSceneObject>("MapRenderer");
    mapRenderer->AddComponent<CoreEngine::TransformComponent>();
    auto* mapView = mapRenderer->AddComponent<GameComponents::MapViewComponent>(
        mapGenerator->GetComponent<GameComponents::MapGeneratorComponent>(),
        groundPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        waterPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        stationPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        rockPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        bananaTreePoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        grassPoolManager->GetComponent<GameComponents::ModelRenderPoolComponent>(),
        gameCamera,
        gridSize, renderWorldDistance);

    // サルが増えた駅を弾ませる。描画は MapView が持つのでここで繋ぐ。
    hungerComponent->SetStationPopCallback(
        [mapView](int32_t stationX, int32_t stationZ) {
            mapView->PlayStationPop(stationX, stationZ);
        });
}

void GameScene::GameScene::OnUpdate() {
}

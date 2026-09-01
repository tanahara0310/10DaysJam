#include "pch.h"
#include "BaseScene.h"
#include "EngineSystem/EngineSystem.h"
#include "Camera/CameraManager.h"
#include "Camera/Camera.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/Render/RenderManager.h"
#include "Scene/SceneManager.h"
#include "UI/UIText.h"
#include "Graphics/Model/ModelManager.h"
#include "Scene/Feature/DefaultSceneFeatures.h"
// GetFeature<T>() で引くために完全型が必要な既定 Feature だけを include する
// （顔ぶれそのものは DefaultSceneFeatures.cpp が持つ）
#include "Scene/Feature/CameraFeature.h"
#include "Scene/Feature/LightingFeature.h"
#include "Scene/Feature/GroundFeature.h"
#include "Scene/Feature/CollisionFeature.h"
#include "Utility/Logger/Logger.h"
#include <algorithm>


namespace CoreEngine
{
    void BaseScene::BuildLoadTasks(StartupSequence& sequence, EngineSystem* engine)
    {
        sequence.Add("カメラと Feature の登録", [this, engine] { SetupSceneCore(engine); });
        sequence.Add("Feature の初期化", [this] { InitializeFeatures(); });
        BuildContentLoadTasks(sequence);
        sequence.Add("モデルの先読み", [this] { BeginModelPreload(); });
        sequence.Add("Feature の後処理", [this] { RunPostSceneInitialize(); });
        sequence.Add("シーンデータの復元", [this] { BeginSceneDataRestore(); });
    }

    void BaseScene::BuildContentLoadTasks(StartupSequence& sequence)
    {
        sequence.Add("オブジェクトの生成", [this] { OnInitialize(); });
    }

    void BaseScene::SetupSceneCore(EngineSystem* engine)
    {
        engine_ = engine;

        // シーン保存システム
        sceneSaveSystem_ = std::make_unique<SceneSaveSystem>();

        // 既定 Feature の登録（顔ぶれは CreateDefaultSceneFeatures() 側）
        RegisterDefaultFeatures();
    }

    void BaseScene::InitializeFeatures()
    {
        // コンテキストは 1 体ごとに取り直す。CameraFeature が生成したカメラを、
        // 後続の Feature が同じループの中で参照できるようにするため。
        for (auto& entry : features_) {
            RefreshFeatureContext();
            entry.feature->Initialize(featureContext_);
        }
        featuresInitialized_ = true;

        // 既定ディレクショナルライトを従来の protected メンバーとして派生クラスへ公開する
        auto* lighting = GetFeature<LightingFeature>();
        directionalLight_ = lighting ? lighting->GetDirectionalLight() : nullptr;
    }

    void BaseScene::RunPostSceneInitialize()
    {
        // OnInitialize() 完了後の Feature フック
        // （シーン生成済みオブジェクトを見る SkyBox の採用判定など）
        RefreshFeatureContext();
        for (auto& entry : features_) {
            entry.feature->PostSceneInitialize(featureContext_);
        }
    }

    void BaseScene::BeginModelPreload()
    {
        auto* modelManager = engine_ ? engine_->GetService<ModelManager>() : nullptr;
        if (!modelManager || !sceneSaveSystem_ || !sceneManager_) {
            return;
        }

        const std::vector<std::string> modelPaths =
            SceneSaveSystem::CollectModelPaths(sceneSaveSystem_->GetSceneName());
        if (modelPaths.empty()) {
            return;
        }

        // ワーカーへ投げて即座に戻る。読み終わるまでの各フレームで画面は回り続ける
        modelManager->BeginPreload(modelPaths);

        sceneManager_->SetLoadStepContinuation(
            [modelManager] {
                const auto progress = modelManager->GetPreloadProgress();
                return progress.first >= progress.second;
            },
            [modelManager] {
                const auto progress = modelManager->GetPreloadProgress();
                return (progress.second == 0)
                    ? 1.0f
                    : static_cast<float>(progress.first) / static_cast<float>(progress.second);
            });
    }

    void BaseScene::BeginSceneDataRestore()
    {
        // 進捗を出す相手（SceneManager）が居ない経路は、その場で読み切る
        if (!sceneManager_) {
            sceneSaveSystem_->Load(&gameObjectManager_);
            return;
        }

        sceneSaveSystem_->BeginLoad(&gameObjectManager_);

        // 1 フレームに 1 体ずつ復元する
        sceneManager_->SetLoadStepContinuation(
            [this] { return sceneSaveSystem_->StepLoad(); },
            [this] { return sceneSaveSystem_->GetLoadProgress(); });
    }

    void BaseScene::Update()
    {
        // フレーム前処理（先頭でカメラ姿勢を確定 → ライト/影・グリッド・デバッグエディタ）
        DispatchUpdate(SceneUpdatePhase::FrameStart);

        // 派生クラスの更新処理（GameObjectの更新前）
        OnUpdate();

        // GameObject 更新前の Feature 更新（床のカメラ追従、最後にトゥイーンの前進）
        DispatchUpdate(SceneUpdatePhase::PreObjectUpdate);

        // ゲームオブジェクトの更新
        gameObjectManager_.UpdateAll();

        // GameObject 更新後の Feature 更新（コリジョン収集 → 判定、最後にイベントの一括配信）
        DispatchUpdate(SceneUpdatePhase::PostObjectUpdate);

        // 派生クラスの後処理（クリーンアップ前）
        OnLateUpdate();

        // 全ロジック確定後の Feature 更新（大気→雲など最新の太陽・カメラ情報の反映）
        DispatchUpdate(SceneUpdatePhase::PostLogic);
    }

    void BaseScene::PrepareRender()
    {
        // cameraManager_ は CameraFeature が GraphicsCore を取れなかった場合に空のままになる。
        // ここでも null を許容すること（描画キューを積まずに抜ける）。
        auto renderManager = engine_->GetService<RenderManager>();
        Camera* activeCamera3D = cameraManager_
            ? cameraManager_->GetActiveCamera(CameraType::Camera3D)
            : nullptr;

        if (!renderManager || !activeCamera3D) {
            return;
        }

        // 全てのゲームオブジェクトを描画キューに追加
        gameObjectManager_.RegisterAllToRender(renderManager);
    }

    void BaseScene::Draw()
    {
        auto* renderManager = engine_->GetService<RenderManager>();
        auto* dxCommon = engine_->GetService<GraphicsCore>();
        if (!renderManager || !dxCommon) {
            return;
        }

        // RenderGraph を経由しないレガシー描画経路。
        // 描画に使うビューは EngineSystem がフレーム先頭で確定済み（FrameViews）なので、
        // ここでアクティブカメラを差し替えない（ギズモ／ピッキングが別カメラを見てズレるため）。
        // RenderContext が無いので、このシーン描画自体がコマンドリストの供給点になる。
        renderManager->SetDebugLineRenderingEnabled(true);
        renderManager->DrawGeometryPass(dxCommon->GetCommandList());
    }

    Camera* BaseScene::GetGameViewCamera3D() const
    {
        // 覗いているカメラの決定は CameraManager に一本化されている（Scene / Game の役割 + フラグ）。
        // シーン側で名前を解決し直すと、また規則が二重化して食い違う。
        return cameraManager_ ? cameraManager_->GetViewCamera() : nullptr;
    }

    Camera* BaseScene::GetGameViewCamera2D() const
    {
        return cameraManager_ ? cameraManager_->GetActiveCamera(CameraType::Camera2D) : nullptr;
    }

    void BaseScene::Finalize()
    {
        // 派生クラス固有の解放
        OnFinalize();

        // Feature の解放（登録の逆順）。
        // CameraFeature は先頭に登録されているのでここでは最後に回り、
        // 他の Feature が解放中も ctx.cameraManager を参照できる。
        RefreshFeatureContext();
        for (auto it = features_.rbegin(); it != features_.rend(); ++it) {
            it->feature->Finalize(featureContext_);
        }

        // ゲームオブジェクトをクリア（新システム）
        gameObjectManager_.Clear();

        // GameObject 破棄後の Feature フック（登録の逆順）。
        // 取り残された購読・再生途中のトゥイーンなど、「オブジェクトの破棄で
        // 解除されるはずだったもの」をここで畳む。
        for (auto it = features_.rbegin(); it != features_.rend(); ++it) {
            it->feature->PostSceneFinalize(featureContext_);
        }

        // Feature を破棄（GetFeature<T>() は以降 nullptr を返す）。
        // カメラ一式の実体もここで消えるので、キャッシュを先に落としておく
        cameraManager_ = nullptr;
        directionalLight_ = nullptr;
        features_.clear();
        featuresInitialized_ = false;
    }

    GameObject* BaseScene::CreateObject(const std::string& name)
    {
        auto obj = std::make_unique<GameObject>();
        obj->SetName(name);
        return gameObjectManager_.AddObject(std::move(obj));
    }

    UIText* BaseScene::CreateText(const std::string& textUtf8, float fontSize,
        UIAnchor anchor, const Vector2& anchoredPos, const Vector4& color,
        const std::string& name)
    {
        // CreateObject<T> の中で Initialize() まで走るので、ここへ来た時点で
        // レンダラーの解決と既定フォントの取得は済んでいる
        auto* text = CreateObject<UIText>();
        if (!text) { return nullptr; }

        if (!name.empty()) {
            text->SetName(name);
        }

        text->SetText(textUtf8);
        text->SetFontSize(fontSize);
        text->SetAnchor(anchor);
        text->SetAnchoredPosition(anchoredPos);
        text->SetColor(color);

        return text;
    }

    ISceneFeature* BaseScene::AddFeature(std::unique_ptr<ISceneFeature> feature, int priority)
    {
        if (!feature) {
            return nullptr;
        }

        FeatureEntry entry;
        entry.feature = std::move(feature);
        entry.priority = priority;
        entry.sequence = featureSequence_++;

        // (priority, 登録順) で決まる位置へ挿入し、features_ を常にソート済みに保つ
        // （RenderPipeline::AddPass と同じ規約）
        auto insertPos = std::find_if(features_.begin(), features_.end(),
            [&entry](const FeatureEntry& existing) {
                return existing.priority > entry.priority;
            });

        ISceneFeature* result = entry.feature.get();
        features_.insert(insertPos, std::move(entry));

        // シーン初期化後（OnInitialize() 内など）の追加は即座に初期化する
        if (featuresInitialized_) {
            RefreshFeatureContext();
            result->Initialize(featureContext_);
        }
        return result;
    }

    void BaseScene::RegisterDefaultFeatures()
    {
        // 顔ぶれと並びは DefaultSceneFeatures.cpp が持つ。
        // 参照が必要になったら GetFeature<T>() で引くので、ここでは持ち回らない。
        for (auto& entry : CreateDefaultSceneFeatures()) {
            AddFeature(std::move(entry.feature), entry.priority);
        }
    }

    void BaseScene::RefreshFeatureContext()
    {
        // カメラ一式は CameraFeature が所有する。初回だけ型で引き、以降はキャッシュを使う
        // （毎フレーム 4 回以上通るので、ここで走査を繰り返さない）。
        if (!cameraManager_) {
            if (auto* camera = GetFeature<CameraFeature>()) {
                cameraManager_ = camera->GetCameraManager();
            }
        }

        featureContext_.engine = engine_;
        featureContext_.gameObjectManager = &gameObjectManager_;
        featureContext_.cameraManager = cameraManager_;
        featureContext_.sceneManager = sceneManager_;
        featureContext_.saveSystem = sceneSaveSystem_.get();
        featureContext_.gameViewCamera3D = GetGameViewCamera3D();
    }

    void BaseScene::DispatchUpdate(SceneUpdatePhase phase)
    {
        // コンテキストは 1 体ごとに取り直す。先頭の CameraFeature が視点を切り替えた
        // フレームでも、後続の Feature が同じフレームで新しい視点カメラを見られるようにする。
        for (auto& entry : features_) {
            RefreshFeatureContext();
            entry.feature->Update(featureContext_, phase);
        }
    }

    void BaseScene::SetReleaseCameraTransform(const Vector3& translate, const Vector3& rotate)
    {
        if (auto* camera = GetFeature<CameraFeature>()) {
            camera->SetReleaseCameraTransform(translate, rotate);
        }
    }

    void BaseScene::SetReleaseCameraLens(float fovDegrees, float farClip, float nearClip)
    {
        if (auto* camera = GetFeature<CameraFeature>()) {
            camera->SetReleaseCameraLens(fovDegrees, farClip, nearClip);
        }
    }

    void BaseScene::SetCollisionEnabled(CollisionLayer a, CollisionLayer b, bool enable)
    {
        if (auto* collision = GetFeature<CollisionFeature>()) {
            collision->SetCollisionEnabled(a, b, enable);
        }
    }

    CollisionWorld* BaseScene::GetCollisionWorld()
    {
        auto* collision = GetFeature<CollisionFeature>();
        return collision ? &collision->GetWorld() : nullptr;
    }

    void BaseScene::SetDefaultGroundEnabled(bool enabled)
    {
        if (auto* ground = GetFeature<GroundFeature>()) {
            ground->SetSuppressed(!enabled);
        }
    }
}

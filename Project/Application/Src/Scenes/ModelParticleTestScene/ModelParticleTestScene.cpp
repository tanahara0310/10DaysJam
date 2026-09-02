#include "pch.h"
#include "ModelParticleTestScene.h"

#include "EngineSystem/EngineSystem.h"
#include "Editor/Environment/AtmosphereEditor.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Graphics/Model/ModelManager.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RHI/Resource/ResourceFactory.h"
#include "Particle/ParticleSystem.h"
#include "Particle/Modules/MainModule.h"
#include "Particle/Modules/EmissionModule.h"
#include "Particle/Modules/ForceModule.h"
#include "Particle/Modules/ShapeModule.h"
#include "Utility/Logger/Logger.h"

namespace ModelParticleTest
{
    using namespace CoreEngine;

    namespace
    {
        /// 検証に使うモデル。半径 1 の球なので、陰影の境目（ターミネータ）が一番読みやすい
        constexpr const char* kModelPath = "sphere.obj";

        /// パーティクルのテクスチャ。真っ白にしておくと色が陰影そのものになる
        /// @note 未設定だとモデルのマテリアルへフォールバックし、それも無ければ
        ///       テクスチャ用ディスクリプタテーブルにハンドル 0 が差さる
        constexpr const char* kParticleTexture = "white1x1.png";

        /// 左（比較元の通常メッシュ）と、右（モデルパーティクル）の配置
        constexpr Vector3 kReferencePosition = { -8.0f, 2.0f, 0.0f };
        constexpr Vector3 kEmitterPosition = { 2.0f, 2.0f, 0.0f };
        constexpr float   kEmitterLineLength = 10.0f;
        constexpr uint32_t kParticleCount = 5;

        /// 太陽の向き。真上から当てるとターミネータが出ないので、低い横からの光にする
        constexpr float kSunElevationDeg = 25.0f;
        constexpr float kSunAzimuthDeg = 90.0f;

        /// 両方が同時に収まる構図。既定レンズ（0.45rad ≒ 25.8°）は狭すぎる
        constexpr Vector3 kCameraPosition = { 0.0f, 3.0f, -20.0f };
        constexpr float   kCameraFovDeg = 45.0f;
        constexpr float   kCameraFarClip = 1000.0f;
    }

    void ModelParticleTestScene::OnInitialize()
    {
        SetSceneName("ModelParticleTestScene");

        // ===== 太陽ライト =====
        // 陰影の向きを左右で比べるので、両者が同じ 1 本のライトを受けるようにする。
        // 輝度スケールは大気散乱シーンの定石に合わせる（TestScene と同じ）。
        if (Light* sun = GetDirectionalLight()) {
            sun->direction = AtmosphereEditor::ComputeSunLightDirection(kSunElevationDeg, kSunAzimuthDeg);
            sun->atmosphereIntensity = 20.0f;
            sun->intensity = kAtmosphereSunIlluminanceLux;
        }

        CreateReferenceSphere();
        CreateModelParticles();

        SetReleaseCameraLens(kCameraFovDeg, kCameraFarClip);
        SetReleaseCameraTransform(kCameraPosition);
    }

    void ModelParticleTestScene::CreateReferenceSphere()
    {
        auto* sphere = CreateObject("ReferenceSphere");
        sphere->AddComponent<MeshRendererComponent>(kModelPath);

        auto& transform = sphere->GetComponent<TransformComponent>()->Get();
        transform.translate = kReferencePosition;
        transform.scale = { 1.0f, 1.0f, 1.0f };

        // マテリアル未指定だと既定値（金属寄り）になり、空を映した青い球になって
        // 拡散反射の比較にならない。白い完全な非金属にして粒側と土俵を揃える。
        auto* material = sphere->AddComponent<MaterialComponent>();
        material->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        material->SetPBR(0.0f, 1.0f, 1.0f);
        material->SetIBLIntensity(0.0f);   // 直接光だけで比べる

        sphere->SetActive(true);
    }

    void ModelParticleTestScene::CreateModelParticles()
    {
        auto* modelManager = engine_->GetService<ModelManager>();
        auto* dxCommon = engine_->GetService<GraphicsCore>();
        auto* resourceFactory = engine_->GetService<ResourceFactory>();
        if (!modelManager || !dxCommon || !resourceFactory) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Graphics,
                "ModelParticleTestScene: 必要なサービスが揃っていないのでパーティクルを作れない");
            return;
        }

        ModelResource* modelResource = modelManager->GetModelResource(kModelPath);
        if (!modelResource) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Graphics,
                "ModelParticleTestScene: モデルを読めなかった: {}", kModelPath);
            return;
        }

        particleSystem_ = CreateObject<ParticleSystem>();
        particleSystem_->Initialize(dxCommon, resourceFactory, "ModelParticles");
        particleSystem_->SetTexture(kParticleTexture);

        // SetModelResource が描画モードを Model へ切り替える（＝ModelParticle パスへ乗る）
        particleSystem_->SetModelResource(modelResource);

        // 既定の加算ブレンドだと陰影が「暗くなる」ではなく「薄くなる」方向に出て読みにくい。
        // 検証では不透明で描く。
        particleSystem_->SetBlendMode(BlendMode::kBlendModeNone);
        particleSystem_->SetEmitterPosition(kEmitterPosition);

        // ===== MainModule =====
        // 撮って比べたいだけなので、動かさず・消えず・その場に留まらせる
        auto& mainData = particleSystem_->GetMainModule().GetMainData();
        // duration を過ぎると MainModule が停止するので、検証中は止まらない長さにする
        mainData.duration = 100000.0f;
        mainData.looping = false;
        mainData.maxParticles = kParticleCount;
        mainData.startLifetime = 100000.0f;   // 検証中は消えないだけの長さ
        mainData.startLifetimeRandomness = 0.0f;
        mainData.startSpeed = 0.0f;
        mainData.startSpeedRandomness = 0.0f;
        mainData.startSize = { 1.0f, 1.0f, 1.0f };  // 比較元の球と同じ半径 1
        mainData.startSizeRandomness = 0.0f;
        mainData.startRotation = { 0.0f, 0.0f, 0.0f };
        mainData.startRotationRandomness = 0.0f;
        mainData.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };  // 白 = 出た色がそのまま陰影
        mainData.startColorRandomness = 0.0f;

        // ===== EmissionModule =====
        // 開始時に 1 回だけ出して、あとは増やさない
        auto& emissionData = particleSystem_->GetEmissionModule().GetEmissionData();
        emissionData.rateOverTime = 0;
        emissionData.burstCount = kParticleCount;
        emissionData.burstTime = 0.0f;

        // ===== ForceModule =====
        // 既定で重力 -9.8 が掛かっており、粒が落ちて位置が毎回変わってしまう。
        // 陰影を比べるのが目的なので静止させる。
        ForceModule::ForceData forceData{};
        forceData.gravity = { 0.0f, 0.0f, 0.0f };
        particleSystem_->GetForceModule().SetForceData(forceData);

        // ===== ShapeModule =====
        // X 方向へ一列に並べて、どの粒も同じ向きの明暗になることを見る
        ShapeModule::ShapeData shapeData{};
        shapeData.shapeType = ShapeModule::ShapeType::Line;
        shapeData.scale.x = kEmitterLineLength;
        shapeData.emissionDirection = { 1.0f, 0.0f, 0.0f };
        particleSystem_->GetShapeModule().SetShapeData(shapeData);

        // MainModule::playOnAwake は値を持つだけで再生を始めるコードが無い
        // （isPlaying_ の既定は両モジュールとも false）。明示的に開始する。
        particleSystem_->Play();
    }
}

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

        /// 比較用の通常メッシュ球（受光の基準）と、爆散の中心
        constexpr Vector3 kReferencePosition = { -10.0f, 2.0f, 0.0f };
        constexpr Vector3 kEmitterPosition = { 0.0f, 4.0f, 0.0f };

        /// 太陽の向き。低い横からの光にして、影が地面に長く伸びるようにする
        constexpr float kSunElevationDeg = 25.0f;
        constexpr float kSunAzimuthDeg = 90.0f;

        /// 爆散のパラメータ
        constexpr uint32_t kParticleCount = 40;
        constexpr float kParticleSize = 0.35f;   ///< sphere.obj は半径 1 なので、そのまま倍率
        constexpr float kBurstSpeed = 4.0f;    ///< 初速。lifetime と掛けた距離が画角に収まる値
        constexpr float kParticleLifetime = 2.0f;
        constexpr float kBurstInterval = 2.5f;    ///< ループ間隔。寿命より少し長くして間を作る

        /// 両方が同時に収まる構図。既定レンズ（0.45rad ≒ 25.8°）は狭すぎる
        constexpr Vector3 kCameraPosition = { 0.0f, 4.0f, -24.0f };
        constexpr float   kCameraFovDeg = 45.0f;
        constexpr float   kCameraFarClip = 1000.0f;
    }

    void ModelParticleTestScene::OnInitialize()
    {
        SetSceneName("ModelParticleTestScene");

        // ===== 太陽ライト =====
        // 比較球と粒が同じ 1 本のライトを受ける構図。
        // 輝度スケールは大気散乱シーンの定石に合わせる（TestScene と同じ）。
        // @note ここでの direction は、CVar "r.AtmosphereLights.SunDirection" が復元されると
        //       AtmosphereEditor::ApplySunSettings に上書きされる（＝シーンからは固定できない）。
        //       影や陰影の向きが想定と違う場合は、エディタの Sky Atmosphere から
        //       仰角・方位角を設定すること。太陽が地平線下に保存されていると全体が暗くなる。
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
        // 一定間隔で中心から爆散させる
        auto& mainData = particleSystem_->GetMainModule().GetMainData();
        mainData.duration = kBurstInterval;
        mainData.looping = true;   // duration ごとにバーストし直す
        mainData.maxParticles = kParticleCount;
        mainData.startLifetime = kParticleLifetime;
        mainData.startLifetimeRandomness = 0.25f;
        mainData.startSpeed = kBurstSpeed;
        mainData.startSpeedRandomness = 0.35f;
        mainData.startSize = { kParticleSize, kParticleSize, kParticleSize };
        mainData.startSizeRandomness = 0.3f;
        mainData.startRotation = { 0.0f, 0.0f, 0.0f };
        mainData.startRotationRandomness = 0.0f;   // 球なので回しても見えない
        mainData.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };  // 白 = 出た色がそのまま陰影
        mainData.startColorRandomness = 0.0f;

        // ===== EmissionModule =====
        // 1 バーストで全量を出し、あとは増やさない（ループのたびに出し直される）
        auto& emissionData = particleSystem_->GetEmissionModule().GetEmissionData();
        emissionData.rateOverTime = 0;
        emissionData.burstCount = kParticleCount;
        emissionData.burstTime = 0.0f;

        // ===== ForceModule =====
        // 弱めの重力で放物線を描かせる。既定の -9.8 だと落下が速すぎて、
        // 影が地面に乗っているところを見る前に落ち切ってしまう。
        ForceModule::ForceData forceData{};
        forceData.gravity = { 0.0f, -3.0f, 0.0f };
        particleSystem_->GetForceModule().SetForceData(forceData);

        // ===== ShapeModule =====
        // 一点（半径 0）から出す。飛ぶ向きは VelocityModule の既定
        // （useRandomDirection）が粒ごとに散らすので、全方向への爆散になる。
        ShapeModule::ShapeData shapeData{};
        shapeData.shapeType = ShapeModule::ShapeType::Sphere;
        shapeData.radius = 0.0f;
        particleSystem_->GetShapeModule().SetShapeData(shapeData);

        // MainModule::playOnAwake は値を持つだけで再生を始めるコードが無い
        // （isPlaying_ の既定は両モジュールとも false）。明示的に開始する。
        particleSystem_->Play();
    }
}

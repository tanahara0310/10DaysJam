#include "pch.h"
#include "RockBreakDebris.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObjectManager.h"
#include "Graphics/Model/ModelManager.h"
#include "Graphics/Model/ModelResource.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RHI/Resource/ResourceFactory.h"
#include "Particle/ParticleSystem.h"
#include "Particle/Modules/CollisionModule.h"
#include "Particle/Modules/ColorModule.h"
#include "Particle/Modules/EmissionModule.h"
#include "Particle/Modules/ForceModule.h"
#include "Particle/Modules/MainModule.h"
#include "Particle/Modules/RotationModule.h"
#include "Particle/Modules/ShapeModule.h"
#include "Particle/Modules/SizeModule.h"
#include "Scene/Feature/ISceneFeature.h"
#include "Utility/Logger/Logger.h"

#include <memory>

using namespace CoreEngine;

namespace {

    // ──────────────────────────────────────────────────────────
    // 破片の調整値
    // ──────────────────────────────────────────────────────────
    // 狙いは「岩が砕けて、欠片が四方へ飛び散る」。岩の中から湧かせて、
    // 隣のマスまで届く勢いで飛ばし、重力で地面へ落とす。
    //
    // 岩は 1 マス（グリッド 1.0）に対して半径 0.56・高さ 0.7〜1.68 で立っている
    // （MapView の rockHeight 0.7 / rockScale 0.7 と rock.obj の大きさから）。

    /// 破片のモデル。particle.obj は 0.4 角のボクセル片
    constexpr const char* kDebrisModel = "particle.obj";

    /// 破片のテクスチャ
    /// @note 未指定だとパーティクルシステム既定の circle.png が乗って白くぼやける。
    ///       モデル付属のパレットを明示的に差す。
    constexpr const char* kDebrisTexture = "particle.png";

    /// 1 回の破壊で出す破片の数
    constexpr uint32_t kChunkCount = 8;

    /// 同時に生かしておける破壊の回数
    /// @details 岩を続けて壊すと投石の飛行時間（0.6 秒）ごとに呼ばれるので、
    ///          前の破片がまだ落ちている途中で次のバーストが来る。その分の余裕。
    constexpr uint32_t kMaxOverlappingBursts = 4;

    /// 着弾位置（岩の足元）から、破片を湧かせる高さ。岩の高さの真ん中あたり
    constexpr float kEmitterHeightOffset = 0.45f;

    /// 湧く範囲の半径。岩の見た目（±0.56）の内側に収める
    constexpr Vector3 kSpawnHalfExtent = { 0.28f, 0.30f, 0.28f };

    /// 破片が消えるまでの時間（秒）
    /// @details 飛ぶ（〜0.4 秒）→ 転がって止まる（〜0.5 秒）→ 少し残ってフェードで消える、
    ///          が収まる長さ。寿命のばらつきで 8 個の消えるタイミングもずらす。
    constexpr float kChunkLifetime = 1.5f;

    /// フェードアウトを始める寿命の割合。ここから寿命の終わりまでで消えていく
    /// @note 1.5 秒 × 残り 30% ＝ おおよそ 0.45 秒かけて消える。
    constexpr float kFadeStartRatio = 0.7f;

    /// 弾け出す速さ [m/s]。空気抵抗と合わせて、遠いものが隣のマス（1.0）を越える程度
    constexpr float kChunkSpeed = 2.8f;

    /// 重力加速度。飛んだ破片が弧を描いて地面へ落ちる強さ
    constexpr float kGravity = -9.0f;

    /// 空気抵抗。散る勢いは残しつつ、飛距離が伸びすぎないように少しだけ効かせる
    constexpr float kDrag = 1.0f;

    /// 空中での回転の速さ [rad/s]。0 を基準に ±この値で振るので、向きも速さも粒ごとに変わる
    /// @note 着地するとここではなく、転がりの回転（横速度から決まる）に切り替わる。
    constexpr float kTumbleSpeed = 8.0f;

    // ── 床との当たり判定 ──
    // 地面ブロックの天面に水平な床を 1 枚張る。高さは MapView の groundHeight（-0.5）と
    // ground.obj の高さ（1.6）× groundScale（0.6）から -0.5 + 0.96 = 0.46。
    // @note 床は 1 枚の平面なので、水のマスや空きマスの上へ飛んだ破片も
    //       この高さで止まる（水面 0.15 より上に乗る）。

    /// 床のワールドY。地面ブロックの天面
    constexpr float kFloorHeight = 0.46f;

    /// 破片の原点から下面までの距離。particle.obj は原点が底面にあり、
    /// 転がると向きで上下するので、埋まりすぎない程度の中間値にしてある
    constexpr float kFloorContactOffset = 0.12f;

    /// 床に当たったときの跳ね返り。1 回小さく跳ねてから転がる程度
    constexpr float kFloorBounce = 0.35f;

    /// 接地中に横速度を削る強さ [1/秒]。転がる距離が 0.5m 前後に収まる値
    constexpr float kFloorFriction = 3.5f;

    /// これ以下の落下速度なら跳ねずに着地させる [m/s]
    constexpr float kFloorRestSpeed = 0.8f;

    /// 転がり半径 [m]。particle.obj の半分の幅
    constexpr float kRollRadius = 0.2f;

    // ──────────────────────────────────────────────────────────
    // Feature
    // ──────────────────────────────────────────────────────────

    class RockBreakDebrisFeature;

    /// いま生きている Feature。PlayRockBreakDebris() の宛先
    /// @note シーンを抜けると null に戻る（タイトル・リザルトでは鳴らさない）
    RockBreakDebrisFeature* g_activeFeature = nullptr;

    /// @brief パーティクルシステムを破片向けに設定する
    void ConfigureModules(ParticleSystem& particleSystem)
    {
        // ===== MainModule =====
        // duration 0 のワンショット。Restart() のたびに 1 バーストだけ出る。
        // duration を 1 フレームより長く取ると、フレーム落ち時にバーストを取りこぼす。
        auto& mainData = particleSystem.GetMainModule().GetMainData();
        mainData.duration = 0.0f;
        mainData.looping = false;
        mainData.playOnAwake = false;
        mainData.maxParticles = kChunkCount * kMaxOverlappingBursts;

        // 8 個が一斉に消えないよう、寿命は粒ごとに大きくばらけさせる
        mainData.startLifetime = kChunkLifetime;
        mainData.startLifetimeRandomness = 0.35f;

        mainData.startSpeed = kChunkSpeed;
        mainData.startSpeedRandomness = 0.5f;

        // particle.obj の大きさをそのまま使う（等倍・ばらつき無し）
        mainData.startSize = { 1.0f, 1.0f, 1.0f };
        mainData.startSizeRandomness = 0.0f;

        // 破片ごとに向きを変える。MainModule のランダムは base ± base×randomness なので、
        // 180 度を基準に 100% 振ると 0〜360 度の一様乱数になる（base が 0 だと振れ幅も 0）。
        mainData.startRotation = { 180.0f, 180.0f, 180.0f };
        mainData.startRotationRandomness = 1.0f;

        // 白 = モデルのテクスチャがそのまま出る
        mainData.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        mainData.startColorRandomness = 0.0f;

        // ForceModule の重力はこの係数と掛け算される。0 のままだと重力が効かない
        mainData.gravityModifier = 1.0f;

        // ===== EmissionModule =====
        // 1 バーストで全量を出し、あとは増やさない
        auto& emissionData = particleSystem.GetEmissionModule().GetEmissionData();
        emissionData.rateOverTime = 0;
        emissionData.burstCount = kChunkCount;
        emissionData.burstTime = 0.0f;

        // ===== ShapeModule =====
        // 岩の体積の中から湧かせる。一点から出すと、砕けたというより噴き出して見える
        ShapeModule::ShapeData shapeData{};
        shapeData.shapeType = ShapeModule::ShapeType::Box;
        shapeData.scale = kSpawnHalfExtent;
        particleSystem.GetShapeModule().SetShapeData(shapeData);

        // ===== ForceModule =====
        // 重力で弧を描かせて地面へ落とす。抵抗は飛距離が伸びすぎない程度に弱く
        ForceModule::ForceData forceData{};
        forceData.gravity = { 0.0f, kGravity, 0.0f };
        forceData.drag = kDrag;
        particleSystem.GetForceModule().SetForceData(forceData);

        // ===== RotationModule =====
        // モデルなので 3 軸で転がす（2D 回転は板ポリ向け）。
        // 速度 0 を基準にランダム幅だけ持たせると、正転と逆転が混ざって砕けた感じになる。
        RotationModule::RotationData rotationData{};
        rotationData.use2DRotation = false;
        rotationData.rotationSpeed = { 0.0f, 0.0f, 0.0f };
        rotationData.rotationSpeedRandomness = { kTumbleSpeed, kTumbleSpeed, kTumbleSpeed };
        particleSystem.GetRotationModule().SetRotationData(rotationData);

        // ===== ColorModule =====
        // 消え際だけアルファを 0 まで落としてフェードアウトさせる。
        // モデルパーティクルは不透明で描くので、アルファはピクセルの間引き率として効く
        // （ModelParticle.PS.hlsl の DitherThreshold）。色そのものは白のまま変えない。
        ColorModule::ColorOverLifetime colorData{};
        colorData.useGradient = true;
        colorData.endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        colorData.startRatio = kFadeStartRatio;
        particleSystem.GetColorModule().SetColorData(colorData);

        // ===== NoiseModule =====
        // 既定で有効（強度 1.0）なので、切らないと着地した破片が床の上で揺れ続ける。
        // 岩の欠片に揺らぎは要らない。
        particleSystem.GetNoiseModule().SetEnabled(false);

        // ===== CollisionModule =====
        // 地面の天面に床を張って、破片を受け止めて転がす。既定は無効なので明示的に入れる。
        particleSystem.GetCollisionModule().SetEnabled(true);
        CollisionModule::CollisionData collisionData{};
        collisionData.planeHeight = kFloorHeight;
        collisionData.contactOffset = kFloorContactOffset;
        collisionData.bounce = kFloorBounce;
        collisionData.friction = kFloorFriction;
        collisionData.restSpeed = kFloorRestSpeed;
        collisionData.roll = true;
        collisionData.rollRadius = kRollRadius;
        particleSystem.GetCollisionModule().SetCollisionData(collisionData);

        // ===== SizeModule =====
        // 大きさは最後まで変えない（既定は寿命で 0 まで縮むので明示的に切る）。
        // 消え際は縮小ではなくフェードアウトで処理する（ColorModule 参照）。
        SizeModule::SizeData sizeData{};
        sizeData.sizeOverLifetime = false;
        particleSystem.GetSizeModule().SetSizeData(sizeData);
    }

    /// @brief 岩の破片を出すモデルパーティクルを、シーンへ 1 つ置いておく Feature
    class RockBreakDebrisFeature final : public ISceneFeature {
    public:
        const char* GetName() const override { return "RockBreakDebris"; }

        void Initialize(SceneContext& context) override
        {
            particleSystem_ = CreateParticleSystem(context);
            if (particleSystem_) {
                g_activeFeature = this;
            }
        }

        /// @brief シーン終了時。GameObject が消える前に受け口を閉じる
        void Finalize(SceneContext&) override
        {
            if (g_activeFeature == this) {
                g_activeFeature = nullptr;
            }
            particleSystem_ = nullptr;
        }

        /// @brief 指定位置へ破片を 1 回分出す
        void Play(const Vector3& impactPosition)
        {
            if (!particleSystem_) {
                return;
            }

            particleSystem_->SetEmitterPosition({
                impactPosition.x,
                impactPosition.y + kEmitterHeightOffset,
                impactPosition.z });

            // Clear() は呼ばない。連続で壊したとき、前の岩の破片を空中で消さずに
            // 落ち切らせる（maxParticles に kMaxOverlappingBursts 回分の余裕がある）。
            particleSystem_->GetMainModule().Restart();
            particleSystem_->GetEmissionModule().Play();
        }

    private:
        /// @brief 破片用のパーティクルシステムを生成する
        /// @return 生成できなければ nullptr（この Feature は以降なにもしない）
        static ParticleSystem* CreateParticleSystem(SceneContext& context);

        /// 破片を出すパーティクルシステム（所有は GameObjectManager）
        ParticleSystem* particleSystem_ = nullptr;
    };

    ParticleSystem* RockBreakDebrisFeature::CreateParticleSystem(SceneContext& context)
    {
        auto* engine = context.engine;
        if (!engine || !context.gameObjectManager) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "RockBreakDebris: シーンのコンテキストが揃っていないので破片を作れません");
            return nullptr;
        }

        auto* dxCommon = engine->GetService<GraphicsCore>();
        auto* resourceFactory = engine->GetService<ResourceFactory>();
        auto* modelManager = engine->GetService<ModelManager>();
        if (!dxCommon || !resourceFactory || !modelManager) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "RockBreakDebris: 必要なサービスが揃っていないので破片を作れません");
            return nullptr;
        }

        // GetModelResource は読み込み済みのものを引くだけで、自分では読まない。
        // particle.obj はこのエフェクト専用で誰も先に読まないので、ここで読ませる。
        modelManager->PreloadModels({ kDebrisModel });

        ModelResource* modelResource = modelManager->GetModelResource(kDebrisModel);
        if (!modelResource) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "RockBreakDebris: モデルを読めませんでした: {}", kDebrisModel);
            return nullptr;
        }

        auto* particleSystem =
            context.gameObjectManager->AddObject(std::make_unique<ParticleSystem>());
        particleSystem->Initialize(dxCommon, resourceFactory, "RockBreakDebris");
        particleSystem->SetTexture(kDebrisTexture);

        // SetModelResource が描画モードを Model へ切り替える（＝ModelParticle パスへ乗る）
        particleSystem->SetModelResource(modelResource);

        // 岩の欠片なので不透明で描く。加算・半透明にすると DXR の影キャスターから
        // 外れて、砕ける前の岩と影の有無が食い違う。
        particleSystem->SetBlendMode(BlendMode::kBlendModeNone);

        ConfigureModules(*particleSystem);
        return particleSystem;
    }
}

std::unique_ptr<CoreEngine::ISceneFeature> GameComponents::CreateRockBreakDebrisFeature()
{
    return std::make_unique<RockBreakDebrisFeature>();
}

void GameComponents::PlayRockBreakDebris(const CoreEngine::Vector3& impactPosition)
{
    if (g_activeFeature) {
        g_activeFeature->Play(impactPosition);
    }
}

#include "pch.h"
#include "GameEntranceFeature.h"

#include "Audio/AudioSystem.h"
#include "Camera/Rig/CameraRig.h"
#include "Camera/Shake/CameraShake.h"
#include "Camera/Shake/CameraShakePresets.h"
#include "Components/Train/TrainMovementComponent.h"
#include "Components/UI/ObjectiveSignComponent.h"
#include "Components/UI/PauseMenuUIComponent.h"
#include "Components/UI/SpeedGaugeUIComponent.h"
#include "Components/UI/StaminaGaugeUIComponent.h"
#include "EngineSystem/EngineSystem.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "GameObject/GameObject.h"
#include "GameObject/GameObjectManager.h"
#include "GameObject/Text3D/Text3DObject.h"
#include "Graphics/PostEffect/Effect/PostEffectManager.h"
#include "Graphics/PostEffect/Effect/PostEffectNames.h"
#include "Graphics/PostEffect/Effect/ToneMapping/ToneMapping.h"
#include "Math/Easing/EasingUtil.h"
#include "RailDirectionGuideFeature.h"
#include "Scene/Feature/ISceneFeature.h"
#include "SkyFogFeature.h"
#include "UI/UIImage.h"
#include "Utility/CVar/CVar.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace CoreEngine;

namespace
{
    /// 真上から見下ろす開幕のリグ。構図は Presets/CameraRigs/Entrance_Sky.json が持つ
    constexpr const char* kSkyRigName = "Entrance_Sky";
    /// 通常のゲーム構図。_camera.json の startupRigName と同じもの
    constexpr const char* kPlayRigName = "GamePlay";
    /// MapViewComponent が置く距離目盛りのオブジェクト名の頭
    constexpr const char* kMarkerNamePrefix = "DistanceMarker";
    /// 看板の入れ物に使う 1x1 の透明画像（ポーズメニューと同じ手）
    constexpr const char* kSignRootTexture = "Application/Assets/Textures/Pause/dim.png";

    constexpr const char* kGoalSePath = "Application/Assets/Sounds/SE/decision.mp3";
    constexpr const char* kCallSePath = "Application/Assets/Sounds/SE/title_bound.mp3";

    /// 借りたフォグを戻し切ったと見なす余白。演出終了後は一切触らない
    constexpr float kFogReleaseMargin = 0.05f;
    /// 1 フレームで進める上限 [秒]。シーン読み込み直後の跳ねで演出が飛ぶのを防ぐ
    constexpr float kMaxStepSeconds = 0.1f;
    /// 到達した目盛りが白から達成色へ落ち着くまでの秒数
    constexpr float kReachedFlashSeconds = 0.6f;
    /// 次の目標の目盛りが脈打つはやさ [rad/秒]
    constexpr float kTargetPulseSpeed = 4.0f;
    /// 脈打ちの振れ幅（1.0 に対する比率）
    constexpr float kTargetPulseAmount = 0.35f;
    /// HUD が 1 枚ずつ遅れて出てくる間隔 [秒]。同時に出すより «次々と揃う» ほうが目で追える
    constexpr float kHudStagger = 0.10f;

    // ──────────────────────────────────────────────────────────
    // 調整用 CVar（CVars.json へ自動保存され、インスペクターの「ゲーム設定」に出る）
    // ──────────────────────────────────────────────────────────
    CVar<bool> cvEnabled{
        "Game.Entrance.Enabled", true,
        "突入演出（雲海ブレイクとカメラの降下）を出すか。"
        "切っても目標看板と目標の進行はそのまま動く" };

    CVar<float> cvCloudSeconds{
        "Game.Entrance.CloudSeconds", 2.05f,
        "雲海が島の下まで沈み切るまでの秒数",
        CVarRange{ 0.2f, 8.0f } };

    CVar<float> cvCloudTopHeight{
        "Game.Entrance.CloudTopHeight", 56.0f,
        "開幕の雲海の高さ [m]。開始カメラ（Entrance_Sky の y）より上にすること。"
        "下げると開幕から島が見えてしまう。この値は CVar 経由で雲へ渡すのではなく、"
        "SkyFogFeature の一時値として渡すので CVars.json へは焼き付かない",
        CVarRange{ 0.0f, 200.0f } };

    CVar<float> cvCloudFalloff{
        "Game.Entrance.CloudFalloff", 0.30f,
        "開幕の雲の柔らかさ（高さ減衰）。小さいほど厚くぼんやりした雲になる",
        CVarRange{ 0.05f, 4.0f } };

    CVar<float> cvCameraDelay{
        "Game.Entrance.CameraDelay", 0.20f,
        "空のリグからゲーム構図へ降り始めるまでの秒数",
        CVarRange{ 0.0f, 4.0f } };

    CVar<float> cvCameraBlendSeconds{
        "Game.Entrance.CameraBlendSeconds", 2.10f,
        "空のリグからゲーム構図へ降り切るまでの秒数",
        CVarRange{ 0.1f, 8.0f } };

    CVar<float> cvSignDelay{
        "Game.Entrance.SignDelay", 2.20f,
        "最初の目標看板が降りてくる時刻 [秒]",
        CVarRange{ 0.0f, 10.0f } };

    CVar<float> cvCallDelay{
        "Game.Entrance.CallDelay", 4.75f,
        "「つなげ！！」を叩き込む時刻 [秒]",
        CVarRange{ 0.0f, 12.0f } };

    CVar<float> cvHudRevealOffset{
        "Game.Entrance.HudRevealOffset", 0.15f,
        "「つなげ！！」から何秒後に HUD（スタミナ・速度計・操作ヒント・レールの矢印）が"
        "出てくるか。演出が終わるまでは引っ込んでいる",
        CVarRange{ 0.0f, 6.0f } };

    CVar<float> cvHudRevealSeconds{
        "Game.Entrance.HudRevealSeconds", 0.55f,
        "HUD が定位置へ滑り込む（矢印は伸び上がる）までの秒数（1 つあたり）",
        CVarRange{ 0.05f, 3.0f } };

    CVar<int> cvGoalStep{
        "Game.Goal.StepMeters", 200,
        "目標距離の刻み [m]。200 なら 200 → 400 → 600 … と続く",
        CVarRange{ 10.0f, 2000.0f } };

    CVar<Vector4> cvReachedColor{
        "Game.Goal.MarkerReachedColor", { 0.02f, 0.22f, 0.03f, 1.0f },
        "到達済みの目標地点の目盛りの色。3D テキストはリニア値なので、"
        "0.25 を超えた成分は昼の露出で白へ飽和する" };

    CVar<Vector4> cvTargetColor{
        "Game.Goal.MarkerTargetColor", { 0.25f, 0.18f, 0.010f, 1.0f },
        "次の目標地点の目盛りの色（脈打つ）。同じくリニア値で入れること" };

    Vector4 Lerp(const Vector4& from, const Vector4& to, float t)
    {
        return { from.x + (to.x - from.x) * t,
                 from.y + (to.y - from.y) * t,
                 from.z + (to.z - from.z) * t,
                 from.w + (to.w - from.w) * t };
    }

    // ──────────────────────────────────────────────────────────
    // Feature
    // ──────────────────────────────────────────────────────────

    /// @brief 突入演出と、200m 刻みの目標提示をまとめて指揮する Feature
    class GameEntranceFeature final : public ISceneFeature
    {
    public:
        const char* GetName() const override { return "GameEntrance"; }

        /// @details シーンの OnInitialize() が終わった後に呼ばれるフックなので、
        ///          この時点なら列車も距離目盛りの持ち主も既に生成されている。
        void PostSceneInitialize(SceneContext& ctx) override
        {
            if (!ctx.gameObjectManager) {
                return;
            }
            engine_ = ctx.engine;
            train_ = ctx.gameObjectManager
                ->FindFirstComponent<GameComponents::TrainMovementComponent>();
            if (!train_) {
                Logger::GetInstance().Warnf(
                    LogCategory::Game,
                    "GameEntranceFeature: 列車が見つからないので目標の進行は止まります");
            }
            // UI もトーンマップ前のバッファへ描かれるので、掛かる露出を打ち消す
            if (auto* postEffects = engine_ ? engine_->GetService<PostEffectManager>() : nullptr) {
                toneMapping_ = postEffects->GetEffect<ToneMapping>(PostEffectNames::ToneMapping);
            }
            if (auto* audioSystem = engine_ ? engine_->GetService<AudioSystem>() : nullptr) {
                audioSystem_ = audioSystem;
            }

            auto* root = ctx.gameObjectManager->AddObject(std::make_unique<UIImage>());
            if (root) {
                root->Initialize(kSignRootTexture, "ObjectiveSignRoot");
                root->SetSerializeEnabled(false);
                sign_ = root->AddComponent<GameComponents::ObjectiveSignComponent>();
            }
            if (!sign_) {
                Logger::GetInstance().Warnf(
                    LogCategory::Game,
                    "GameEntranceFeature: 目標看板を作れませんでした");
            }

            goalMeters_ = GoalStep();

            // 雲はここでは触らない。最初の Update（FrameStart）で入れる。
            //
            // ここで持ち上げてしまうと、ローディングが終わって最初のフレームが回るまで
            // 雲の高さが 56m のまま静止する。SkyFogFeature はこの値を r.Fog.HeightRef へ
            // 流し込み、CVar の自動保存は「最後の変更から 0.3 秒後」に走るので、
            // その静止中に必ず 56 が CVars.json へ焼き付く。そうなると次回起動から
            // タイトル画面が雲の中＝真っ白で始まる（実際にそれを踏んだ）。
            //
            // 演出が動いている間は毎フレーム値が変わってデバウンスが張り直されるため、
            // 保存が走るのは掃引が終わって通常値へ戻った 0.3 秒後になる。
            // GameEntranceFeature は SkyFogFeature より先に登録してあるので、
            // 1 フレーム目の FrameStart で入れれば描画には間に合う。
        }

        void Update(SceneContext& ctx, SceneUpdatePhase phase) override
        {
            if (phase == SceneUpdatePhase::FrameStart) {
                UpdateEntrance(ctx);
                UpdateGoal();
                return;
            }
            if (phase == SceneUpdatePhase::PostLogic) {
                // 目盛りの色は MapView が書いた後に上書きする。
                // MapView は生成時に一度だけ白を入れるので、毎フレーム上書きしても衝突しない
                UpdateDistanceMarkers(ctx);
            }
        }

        void Finalize(SceneContext&) override
        {
            // 演出の途中でシーンを抜けても、雲は必ず通常へ戻す
            GameComponents::SetSkyFogCloudLift(0.0f, 0.0f, 1.0f);
            cloudLifted_ = false;
        }

    private:
        int GoalStep() const { return std::max(1, cvGoalStep.Get()); }

        /// @brief 雲海の高さと柔らかさを、演出の進み具合に合わせて渡す
        /// @param progress 0 = 開幕（雲の中）／1 = 通常のフォグへ戻り切った
        /// @details 渡す先は SkyFogFeature の一時値で、CVar は 1 つも書き換えない。
        ///          CVar を毎フレーム動かすと自動保存が演出の途中で走り、掃引中の値が
        ///          CVars.json へ焼き付いて、次回起動のタイトルが雲の中で始まる。
        void ApplyCloud(float progress)
        {
            const float eased =
                EasingUtil::Apply(std::clamp(progress, 0.0f, 1.0f),
                                  EasingUtil::Type::EaseInOutCubic);
            GameComponents::SetSkyFogCloudLift(
                1.0f - eased, cvCloudTopHeight.Get(), cvCloudFalloff.Get());
            cloudLifted_ = eased < 1.0f;
        }

        void UpdateEntrance(SceneContext& ctx)
        {
            if (sign_) {
                sign_->SetExposureScale(
                    toneMapping_ ? std::exp2(-toneMapping_->GetAutoExposureEV()) : 1.0f);
            }
            if (entranceDone_) {
                return;
            }
            const bool playCinematic = cvEnabled.Get();
            if (playCinematic && !cloudStarted_) {
                // 1 フレーム目。まだ時間を進める前に雲を持ち上げて、この frame から雲の中にする
                cloudStarted_ = true;
                ApplyCloud(0.0f);
            }
            elapsed_ += std::clamp(Time::DeltaTime(), 0.0f, kMaxStepSeconds);

            // ---- 雲海ブレイク ----
            if (playCinematic) {
                const float cloudSeconds = std::max(0.01f, cvCloudSeconds.Get());
                const float progress = elapsed_ / cloudSeconds;
                if (progress < 1.0f) {
                    ApplyCloud(progress);
                } else if (cloudLifted_) {
                    ApplyCloud(1.0f);   // 通常のフォグへ戻し切る
                }

                // ---- カメラ：真上のリグ → ゲーム構図 ----
                if (!skyRigStarted_) {
                    // 起動時のリグ（_camera.json の startupRigName）より確実に後に
                    // 割り込むため、初期化フックではなく最初の更新で握る
                    skyRigStarted_ = true;
                    if (!CameraRig::Activate(kSkyRigName)) {
                        Logger::GetInstance().Warnf(
                            LogCategory::Game,
                            "GameEntranceFeature: リグ {} が無いのでカメラ演出は飛ばします",
                            kSkyRigName);
                        playRigStarted_ = true;   // 降下も要らない
                    }
                }
                if (!playRigStarted_ && elapsed_ >= cvCameraDelay.Get()) {
                    playRigStarted_ = true;
                    CameraRigActivateOptions options;
                    options.blendSeconds = std::max(0.01f, cvCameraBlendSeconds.Get());
                    CameraRig::Activate(kPlayRigName, options);
                }
            }

            // ---- もくひょう看板 ----
            const float signDelay = playCinematic ? cvSignDelay.Get() : 0.3f;
            if (!signShown_ && elapsed_ >= signDelay) {
                signShown_ = true;
                if (sign_) {
                    sign_->Show(static_cast<std::uint32_t>(goalMeters_));
                }
            }

            // ---- つなげ！！ ----
            const float callDelay = playCinematic ? cvCallDelay.Get() : 1.6f;
            if (!callPlayed_ && elapsed_ >= callDelay) {
                callPlayed_ = true;
                if (sign_) {
                    sign_->PlayStartCall();
                }
                CameraShake::Play(CameraShakePresets::Landing());
                PlaySe(kCallSePath);
            }

            // ---- HUD の登場 ----
            UpdateHudReveal(ctx, callDelay);

            if (callPlayed_ && hudRevealDone_ && !cloudLifted_
                && elapsed_ >= callDelay + kFogReleaseMargin) {
                entranceDone_ = true;
            }
        }

        /// @brief HUD とレールの矢印を演出中は隠し、締めの合図に合わせて出す
        /// @param callDelay 「つなげ！！」の時刻 [秒]。ここを基準に出す
        /// @details 各 HUD は自分の板幅から寄せ幅を決めるので、ここが渡すのは進み具合だけ。
        ///          EaseOutBack を通した値をそのまま渡して、行き過ぎて戻る手応えを出す。
        void UpdateHudReveal(SceneContext& ctx, float callDelay)
        {
            if (hudRevealDone_) {
                return;
            }
            // HUD は各 Feature の PostSceneInitialize で作られる。こちらのほうが
            // 登録が先なので、初期化時点では見つからない。毎フレーム引き直す
            if (!stamina_ && ctx.gameObjectManager) {
                stamina_ = ctx.gameObjectManager
                    ->FindFirstComponent<GameComponents::StaminaGaugeUIComponent>();
            }
            if (!speedGauge_ && ctx.gameObjectManager) {
                speedGauge_ = ctx.gameObjectManager
                    ->FindFirstComponent<GameComponents::SpeedGaugeUIComponent>();
            }
            if (!pauseMenu_ && ctx.gameObjectManager) {
                pauseMenu_ = ctx.gameObjectManager
                    ->FindFirstComponent<GameComponents::PauseMenuUIComponent>();
            }

            const float start = callDelay + cvHudRevealOffset.Get();
            const float span = std::max(0.05f, cvHudRevealSeconds.Get());
            const auto revealAt = [&](float stagger) {
                const float t =
                    std::clamp((elapsed_ - start - stagger) / span, 0.0f, 1.0f);
                return EasingUtil::Apply(t, EasingUtil::Type::EaseOutBack);
            };

            // 床の矢印はワールド側なので、HUD の 1 枚目と同時でも読み分けられる
            GameComponents::SetRailDirectionGuideReveal(revealAt(0.0f));
            if (stamina_) { stamina_->SetIntroReveal(revealAt(0.0f)); }
            if (speedGauge_) { speedGauge_->SetIntroReveal(revealAt(kHudStagger)); }
            if (pauseMenu_) { pauseMenu_->SetIntroReveal(revealAt(kHudStagger * 2.0f)); }

            if (elapsed_ >= start + kHudStagger * 2.0f + span) {
                hudRevealDone_ = true;
            }
        }

        /// @brief 列車が目標を越えたら、目盛りへ色を付けて次の目標の看板を出す
        void UpdateGoal()
        {
            if (!train_) {
                return;
            }
            reachedFlash_ += std::clamp(Time::DeltaTime(), 0.0f, kMaxStepSeconds);

            // 地面の目盛りはワールド X をそのままメートルとして置かれている。
            // 列車の絶対位置で見るので、看板の数字と足元の数字が必ず一致する
            const float worldX = train_->GetWorldPosition().x;
            const int step = GoalStep();
            bool advanced = false;
            while (worldX >= static_cast<float>(goalMeters_)) {
                reachedMeters_ = goalMeters_;
                goalMeters_ += step;
                advanced = true;
            }
            if (!advanced) {
                return;
            }
            reachedFlash_ = 0.0f;
            if (sign_) {
                sign_->Show(static_cast<std::uint32_t>(goalMeters_));
            }
            PlaySe(kGoalSePath);
            Logger::GetInstance().Infof(
                LogCategory::Game,
                "GameEntrance: {}m 到達。つぎの目標は {}m", reachedMeters_, goalMeters_);
        }

        /// @brief 目標地点の目盛りだけ色を差し替える（それ以外は白のまま）
        void UpdateDistanceMarkers(SceneContext& ctx)
        {
            if (!ctx.gameObjectManager) {
                return;
            }
            const int step = GoalStep();
            const Vector4 plain{ 1.0f, 1.0f, 1.0f, 1.0f };
            const Vector4 reached = cvReachedColor.Get();

            // 次の目標は脈打たせて「ここを目指す」と分かるようにする
            const float pulse = 1.0f
                + std::sin(pulseTimer_ * kTargetPulseSpeed) * kTargetPulseAmount;
            const Vector4 target = [&] {
                const Vector4 c = cvTargetColor.Get();
                return Vector4{ c.x * pulse, c.y * pulse, c.z * pulse, c.w };
            }();
            // 到達した瞬間だけ白く光らせ、達成色へ落ち着かせる
            const float flash =
                std::clamp(reachedFlash_ / kReachedFlashSeconds, 0.0f, 1.0f);
            const Vector4 justReached = Lerp(plain, reached, flash);

            pulseTimer_ += std::clamp(Time::DeltaTime(), 0.0f, kMaxStepSeconds);

            for (const auto& object : ctx.gameObjectManager->GetAllObjects()) {
                if (!object || object->GetName().rfind(kMarkerNamePrefix, 0) != 0) {
                    continue;
                }
                auto* marker = dynamic_cast<Text3DObject*>(object.get());
                auto* transform = marker ? marker->GetComponent<TransformComponent>() : nullptr;
                if (!transform) {
                    continue;
                }
                const int meters =
                    static_cast<int>(std::lround(transform->Get().translate.x));
                if (meters <= 0 || meters % step != 0) {
                    marker->SetColor(plain);
                    continue;
                }
                if (meters == reachedMeters_) {
                    marker->SetColor(justReached);
                } else if (meters <= reachedMeters_) {
                    marker->SetColor(reached);
                } else if (meters == goalMeters_) {
                    marker->SetColor(target);
                } else {
                    marker->SetColor(plain);
                }
            }
        }

        void PlaySe(const char* path) const
        {
            if (audioSystem_) {
                audioSystem_->PlayOneShot(path, { .bus = AudioBus::SE });
            }
        }

        EngineSystem* engine_ = nullptr;
        AudioSystem* audioSystem_ = nullptr;
        ToneMapping* toneMapping_ = nullptr;
        GameComponents::TrainMovementComponent* train_ = nullptr;
        GameComponents::ObjectiveSignComponent* sign_ = nullptr;
        GameComponents::StaminaGaugeUIComponent* stamina_ = nullptr;
        GameComponents::SpeedGaugeUIComponent* speedGauge_ = nullptr;
        GameComponents::PauseMenuUIComponent* pauseMenu_ = nullptr;

        float elapsed_ = 0.0f;
        float pulseTimer_ = 0.0f;
        float reachedFlash_ = kReachedFlashSeconds;
        int goalMeters_ = 200;
        int reachedMeters_ = 0;

        bool cloudStarted_ = false;
        bool cloudLifted_ = false;
        bool skyRigStarted_ = false;
        bool playRigStarted_ = false;
        bool signShown_ = false;
        bool callPlayed_ = false;
        bool hudRevealDone_ = false;
        bool entranceDone_ = false;
    };
}

std::unique_ptr<CoreEngine::ISceneFeature> GameComponents::CreateGameEntranceFeature()
{
    return std::make_unique<GameEntranceFeature>();
}

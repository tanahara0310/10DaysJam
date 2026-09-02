#include "pch.h"
#include "EngineSystem/EngineSystem.h"
#include "Input/InputManager.h"
#include "Graphics/Model/ModelManager.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"

#ifdef _DEBUG
#include "Editor/Camera/CameraDebugUI.h"
#endif

#include "TestScene.h"
#include "WinApp/WinApp.h"
#include "Scene/SceneManager.h"
#include "Graphics/Render/RenderManager.h"
#include "Editor/Environment/AtmosphereEditor.h"
#include "Utility/FrameRate/Time.h"

#include <cstdlib>
#include <format>
#include <iostream>

using namespace CoreEngine::MathCore;

namespace
{
    // Assets に音を置いていないので Windows 標準の WAV を直接指す。
    // AudioPathResolver はドライブレター付きのパスをそのまま通すので、
    // Assets 配下と同じ書き味で絶対パスも渡せる
    constexpr const char* kSeSoundPath = "C:/Windows/Media/Windows Notify System Generic.wav";
    constexpr const char* kBgmSoundPath = "C:/Windows/Media/Ring01.wav";
}

// アプリケーションの初期化

namespace CoreEngine
{
    void TestScene::OnInitialize()
    {
        SetSceneName("TestScene");

        // オーディオの動作確認（結果はログの Audio カテゴリに出る）
        InitializeAudioTest();

        ///========================================================
        // モデルの読み込みと初期化
        ///========================================================

        // コンポーネントを直接取得
        auto modelManager = engine_->GetService<ModelManager>();

        if (!modelManager) {
            return; // 必須コンポーネントがない場合は終了
        }

        // ===== モデルリソースを並列プリロード =====
        // 全モデルを事前にバックグラウンドスレッドで並列読み込みし、
        // 後続の CreateObject 時にはキャッシュヒットで即座に返る
        //modelManager->PreloadModels({ "sphere.obj" });

        // ===== 太陽ライト =====
        // 空は BaseScene が既定で大気散乱モードの SkyBox（＋雲）を自動生成するため、
        // 以前の静的 HDR キューブマップ＋IBL セットアップは廃止した。
        // 球体の環境反射は大気の空キューブマップ（スペキュラIBL）が担う。
        // LightingFeature の既定値（天頂・シェーダー単位 intensity=1 相当）は大気散乱が
        // 期待する輝度スケールと整合しないため明示的に上書きする（他の大気シーンと同じ定石）。
        if (Light* sun = GetDirectionalLight()) {
            sun->direction = AtmosphereEditor::ComputeSunLightDirection(35.0f, 25.0f);
            // 空（大気・雲）の輝度スケールと、サーフェスの直接光は単位系が別なので分離して与える
            sun->atmosphereIntensity = 20.0f;
            sun->intensity = kAtmosphereSunIlluminanceLux;
        }

        // ===== PBR パラメータテスト用球体グリッド =====
        // 列（X 軸）: Roughness  0.0（左=鏡面） → 1.0（右=粗面）
        // 行（Y 軸）: Metallic   0.0（下=非金属） → 1.0（上=金属）
        //
        //  Metallic
        //  1.0 ┌──────────────────────────────────┐
        //      │  金属 × 各 Roughness              │
        //  0.5 │  半金属 × 各 Roughness            │
        //  0.0 └──────────────────────────────────┘
        //      0.0   0.17  0.33  0.5  0.67  0.83  1.0  Roughness

        constexpr int   kRoughnessSteps = 7;   // 列数（Roughness 軸）
        constexpr int   kMetallicSteps = 7;   // 行数（Metallic 軸）
        constexpr float kSpacing = 2.5f; // 球体間の間隔

        // グリッド原点。X は中央が座標原点になるよう計算する。
        // Y は既定の無限遠タイル床（y=0）より上にグリッド全体が載るよう下端を持ち上げる。
        constexpr float kGridBaseY = 1.5f;   // 最下段の中心高さ（球半径 1 を考慮して床に埋まらない）
        const float originX = -(kRoughnessSteps - 1) * kSpacing * 0.5f;
        const float originY = kGridBaseY;

        for (int row = 0; row < kMetallicSteps; ++row)
        {
            const float metallic = static_cast<float>(row) / static_cast<float>(kMetallicSteps - 1);

            for (int col = 0; col < kRoughnessSteps; ++col)
            {
                const float roughness = static_cast<float>(col) / static_cast<float>(kRoughnessSteps - 1);

                // コンポーネント合成で組む（専用クラスは不要）
                auto* sphere = CreateObject("Sphere");
                // sphere.obj は Assets に無いので、既存モデルで PBR グリッドを組む
                sphere->AddComponent<MeshRendererComponent>("monkey.obj");

                auto& transform = sphere->GetComponent<TransformComponent>()->Get();
                transform.translate = {
                    originX + col * kSpacing,
                    originY + row * kSpacing,
                    0.0f
                };
                transform.scale = { 1.0f, 1.0f, 1.0f };

                auto* material = sphere->AddComponent<MaterialComponent>();
                material->SetPBR(metallic, roughness, 1.0f);
                material->SetIBLIntensity(1.0f);

                sphere->SetActive(true);
            }
        }

        // グリッドは床の上（y=1.5〜16.5）に持ち上がったため、その中心を正面に捉える
        const float gridCenterY = kGridBaseY + (kMetallicSteps - 1) * kSpacing * 0.5f;
        SetReleaseCameraTransform({ 0.0f, gridCenterY, -30.0f });
    }

    void TestScene::OnUpdate()
    {
        auto inputManager = engine_->GetService<InputManager>();
        if (!inputManager) {
            return;
        }

        // Tabキーでテストシーンをリスタート
        auto& input = inputManager->GetQuery();
        if (input.IsKeyTriggered(DIK_TAB)) {
            if (sceneManager_) {
                sceneManager_->ChangeScene("TestScene");
            }
            return;
        }

        RunAudioAutoCheck();
        UpdateAudioTest();
    }

    // ============================================================
    // オーディオ動作確認
    //
    //   1 : SE を単発再生（連打すると重なって鳴る＝旧実装で切れていた挙動の確認）
    //   2 : ピッチをばらつかせて SE を再生
    //   3 : BGM を 2 秒でフェードアウトして停止
    //   4 : BGM を 2 秒でフェードインして再開
    //   5 / 6 : BGM バスの音量を 0.1 下げる / 上げる
    //   7 / 8 : SE バスの音量を 0.1 下げる / 上げる
    //   9 : マスター音量を 0.1 下げる（0 まで下がったら 1.0 に戻す）
    //   0 : 鳴っている音を全部止める
    //   P : 現在の状態をログに出す
    // ============================================================

    void TestScene::InitializeAudioTest()
    {
        audio_ = engine_->GetService<AudioSystem>();
        if (!audio_) {
            logger.Logf(LogLevel::Error, LogCategory::Audio, "{}",
                "[AudioTest] AudioSystem service not registered");
            return;
        }

        seClip_ = audio_->LoadClip(kSeSoundPath);
        bgmClip_ = audio_->LoadClip(kBgmSoundPath);

        logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
            std::format("[AudioTest] LoadClip se={} bgm={}",
                seClip_.IsValid() ? "OK" : "FAILED",
                bgmClip_.IsValid() ? "OK" : "FAILED"));

        // 同じパスを読み直したときに PCM を共有しているか（キャッシュの確認）
        const SoundClip reloaded = audio_->LoadClip(kSeSoundPath);
        logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
            std::format("[AudioTest] clip cache: {}",
                (reloaded == seClip_) ? "HIT (same clip shared)" : "MISS (decoded twice)"));

        // BGM は BGM バスへ。ScopedSound なのでシーンが壊れれば自動で止まる
        bgm_ = audio_->PlayScoped(bgmClip_, {
            .bus = AudioBus::BGM,
            .loop = true,
            .volume = 0.5f,
            .fadeInTime = 2.0f,
            });

        logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
            std::format("[AudioTest] BGM start: playing={} fading={}",
                bgm_->IsPlaying(), bgm_->IsFading()));

        LogAudioState("after init");
    }

    void TestScene::UpdateAudioTest()
    {
        if (!audio_) {
            return;
        }

        auto& input = engine_->GetService<InputManager>()->GetQuery();

        // --- SE（重ねて鳴ることの確認）---
        if (input.IsKeyTriggered(DIK_1)) {
            audio_->PlayOneShot(seClip_, { .bus = AudioBus::SE, .volume = 0.8f });
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AudioTest] SE one-shot / playing count = {}", audio_->GetPlayingCount()));
        }
        if (input.IsKeyTriggered(DIK_2)) {
            // 0.7〜1.3 倍でばらつかせる。連打しても互いを切らない
            const float pitch = 0.7f + static_cast<float>(std::rand() % 61) * 0.01f;
            audio_->PlayOneShot(seClip_, { .bus = AudioBus::SE, .volume = 0.8f, .pitch = pitch });
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AudioTest] SE one-shot pitch={:.2f} / playing count = {}",
                    pitch, audio_->GetPlayingCount()));
        }

        // --- BGM のフェード ---
        if (input.IsKeyTriggered(DIK_3)) {
            bgm_->FadeOut(2.0f);
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}", "[AudioTest] BGM fade out (2.0s)");
        }
        if (input.IsKeyTriggered(DIK_4)) {
            bgm_ = audio_->PlayScoped(bgmClip_, {
                .bus = AudioBus::BGM,
                .loop = true,
                .volume = 0.5f,
                .fadeInTime = 2.0f,
                });
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}", "[AudioTest] BGM fade in (2.0s)");
        }

        // --- バス音量 ---
        if (input.IsKeyTriggered(DIK_5)) {
            audio_->SetBusVolume(AudioBus::BGM, audio_->GetBusVolume(AudioBus::BGM) - 0.1f);
            LogAudioState("BGM bus down");
        }
        if (input.IsKeyTriggered(DIK_6)) {
            audio_->SetBusVolume(AudioBus::BGM, audio_->GetBusVolume(AudioBus::BGM) + 0.1f);
            LogAudioState("BGM bus up");
        }
        if (input.IsKeyTriggered(DIK_7)) {
            audio_->SetBusVolume(AudioBus::SE, audio_->GetBusVolume(AudioBus::SE) - 0.1f);
            LogAudioState("SE bus down");
        }
        if (input.IsKeyTriggered(DIK_8)) {
            audio_->SetBusVolume(AudioBus::SE, audio_->GetBusVolume(AudioBus::SE) + 0.1f);
            LogAudioState("SE bus up");
        }

        // --- マスター音量 ---
        if (input.IsKeyTriggered(DIK_9)) {
            const float next = audio_->GetMasterVolume() - 0.1f;
            audio_->SetMasterVolume(next < 0.05f ? 1.0f : next);
            LogAudioState("master volume");
        }

        // --- 全停止 ---
        if (input.IsKeyTriggered(DIK_0)) {
            audio_->StopAll();
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AudioTest] StopAll / playing count = {}", audio_->GetPlayingCount()));
        }

        if (input.IsKeyTriggered(DIK_P)) {
            LogAudioState("manual dump");
        }
    }

    void TestScene::RunAudioAutoCheck()
    {
        // 各ステップを回す時刻（秒）。BGM のフェードインは 2.0 秒で設定してある
        static constexpr float kStepTimes[] = {
            0.5f,  // フェード途中
            1.0f,  // フェード途中
            2.5f,  // フェード完了後
            3.0f,  // SE 1 発目
            3.2f,  // SE 2 発目（1 発目がまだ鳴っている）
            3.4f,  // SE 3 発目（1・2 発目がまだ鳴っている）
            6.0f,  // SE が鳴り終わった後（スロットが回収されているか）
            6.5f,  // BGM バスを絞る
            7.0f,  // BGM バスを戻す
        };
        static constexpr int kStepCount = static_cast<int>(std::size(kStepTimes));

        if (!audio_ || audioCheckStep_ >= kStepCount) {
            return;
        }

        audioCheckTimer_ += Time::UnscaledDeltaTime();
        if (audioCheckTimer_ < kStepTimes[audioCheckStep_]) {
            return;
        }

        switch (audioCheckStep_) {
        case 0:
        case 1:
            // フェードインが実際に進んでいるか（旧実装はここが一切動かなかった）
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s BGM fading={} volume={:.3f}",
                    audioCheckTimer_, bgm_->IsFading(), bgm_->GetVolume()));
            break;

        case 2:
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s fade done: fading={} volume={:.3f} (expect 0.500)",
                    audioCheckTimer_, bgm_->IsFading(), bgm_->GetVolume()));
            break;

        case 3:
        case 4:
        case 5:
            // 同じ SE を短い間隔で重ねる。旧実装は Play のたびに前の音を切っていた
            audio_->PlayOneShot(seClip_, { .bus = AudioBus::SE, .volume = 0.6f });
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s SE #{} fired / playing={} (BGM 1 + SE {})",
                    audioCheckTimer_, audioCheckStep_ - 2,
                    audio_->GetPlayingCount(), audioCheckStep_ - 2));
            break;

        case 6:
            // 鳴り終わった SE のスロットが Update() で回収されていれば BGM の 1 本だけ残る
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s after SE finished: playing={} (expect 1 = BGM only)",
                    audioCheckTimer_, audio_->GetPlayingCount()));
            break;

        case 7:
            audio_->SetBusVolume(AudioBus::BGM, 0.3f);
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s BGM bus -> {:.2f} (source volume stays {:.3f})",
                    audioCheckTimer_, audio_->GetBusVolume(AudioBus::BGM), bgm_->GetVolume()));
            break;

        case 8:
            audio_->SetBusVolume(AudioBus::BGM, 1.0f);
            logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
                std::format("[AutoCheck] t={:.1f}s BGM bus restored -> {:.2f}",
                    audioCheckTimer_, audio_->GetBusVolume(AudioBus::BGM)));
            LogAudioState("auto check done");
            break;

        default:
            break;
        }

        ++audioCheckStep_;
    }

    void TestScene::LogAudioState(const char* label) const
    {
        if (!audio_) {
            return;
        }

        logger.Logf(LogLevel::Info, LogCategory::Audio, "{}",
            std::format("[AudioTest] {} | playing={} master={:.2f} "
                        "bus(BGM vol={:.2f} duck={:.2f}) bus(SE vol={:.2f} duck={:.2f}) "
                        "bgm(valid={} playing={} volume={:.2f})",
                label,
                audio_->GetPlayingCount(),
                audio_->GetMasterVolume(),
                audio_->GetBusVolume(AudioBus::BGM), audio_->GetBusDuck(AudioBus::BGM),
                audio_->GetBusVolume(AudioBus::SE), audio_->GetBusDuck(AudioBus::SE),
                bgm_->IsValid(), bgm_->IsPlaying(), bgm_->GetVolume()));
    }

}



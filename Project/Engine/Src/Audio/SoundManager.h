#pragma once

// ──────────────────────────────────────────────────────────
// 旧 SoundManager 互換ヘッダ
//
// 実体は AudioSystem に移った。このヘッダは既存の呼び出し側を無変更のまま
// 通すためだけに残してある（Phase 4 で削除する）。
//
// 新規コードは Audio/AudioSystem.h を直接 include し、
// LoadClip() + Play() / PlayOneShot() / ScopedSound を使うこと。
// ──────────────────────────────────────────────────────────

#include <memory>
#include <string>

#include "Audio/AudioSystem.h"

namespace CoreEngine
{
    /// @brief 旧クラス名。AudioSystem のエイリアス
    /// @details 型エイリアスなので `engine->GetService<SoundManager>()` は
    ///          `GetService<AudioSystem>()` と同じサービスを引く。
    using SoundManager = AudioSystem;

    /// @deprecated 旧ハンドル型。互換のためだけに残している
    using SoundHandle = size_t;

    /// @brief 旧 SoundManager 時代の RAII サウンドハンドル
    /// @deprecated AudioSystem::LoadClip() + Play()、または ScopedSound を使うこと。
    /// @details 内部は SoundClip（波形）+ SoundInstance（再生）へ委譲するだけの薄い層。
    ///          旧実装との差分:
    ///          - 未再生のうちに SetVolume() した値を GetVolume() が返す
    ///            （旧実装は pendingVolume_ を参照しておらず 0 を返していた）
    ///          - フェードは AudioSystem::Update() が進めるので UpdateFade() は不要
    ///          - AudioSystem が先に破棄されてもデストラクタが落ちない
    class AudioSystem::SoundResource {
    public:
        SoundResource(AudioSystem* system, SoundClip clip, AudioBus bus);
        ~SoundResource();

        // コピー禁止、ムーブ可能
        SoundResource(const SoundResource&) = delete;
        SoundResource& operator=(const SoundResource&) = delete;
        SoundResource(SoundResource&& other) noexcept;
        SoundResource& operator=(SoundResource&& other) noexcept;

        /// @brief 先頭から再生する（再生中なら鳴らし直す）
        bool Play(bool loop = false);
        void Stop();
        void Pause();
        void Resume();

        void SetVolume(float volume);
        float GetVolume() const;
        void SetPitch(float pitch);
        float GetPitch() const;

        bool IsPlaying() const;
        bool IsPaused() const;

        /// @brief 音量 0 から目標音量へフェードイン（再生中のみ有効）
        void FadeIn(float duration, float targetVolume = 1.0f);
        /// @brief 現在の音量から 0 へフェードアウト
        void FadeOut(float duration, bool stopAfterFade = true);
        bool IsFading() const;

        /// @deprecated 何もしない。フェードは AudioSystem::Update() が進める
        void UpdateFade(float /*deltaTime*/) {}

        /// @brief 読み込み済みのサウンドを保持しているか
        bool IsValid() const { return system_ != nullptr && clip_.IsValid(); }

    private:
        /// @brief 再生中なら停止してハンドルを手放す（AudioSystem 破棄後は何もしない）
        void ReleaseInstance();

        AudioSystem* system_ = nullptr;
        SoundClip clip_;
        SoundInstance instance_;
        AudioBus bus_ = AudioBus::SE;
        std::weak_ptr<void> systemLifetime_;

        // 未再生の間に Set された値を覚えておく。旧実装はこれを pendingVolume_ /
        // pendingPitch_ という別テーブルで持ち、Get 側が参照していなかったため
        // 「SetVolume した直後の GetVolume が 0 を返す」状態になっていた
        float volume_ = 1.0f;
        float pitch_ = 1.0f;
    };
}

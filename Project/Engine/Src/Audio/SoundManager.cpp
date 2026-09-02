#include "pch.h"
#include "SoundManager.h"

#include <algorithm>

namespace CoreEngine
{
    // ──────────────────────────────────────────────────────────
    // 旧 SoundManager::SoundResource（互換層）
    //
    // SoundClip（波形）と SoundInstance（再生）へ委譲するだけの薄いラッパ。
    // Phase 4 でこのファイルごと削除する。
    // ──────────────────────────────────────────────────────────

    AudioSystem::SoundResource::SoundResource(AudioSystem* system, SoundClip clip, AudioBus bus)
        : system_(system)
        , clip_(clip)
        , bus_(bus)
        , systemLifetime_(system ? system->GetLifetimeToken() : std::weak_ptr<void>{})
    {
    }

    AudioSystem::SoundResource::~SoundResource()
    {
        ReleaseInstance();
    }

    AudioSystem::SoundResource::SoundResource(SoundResource&& other) noexcept
        : system_(other.system_)
        , clip_(other.clip_)
        , instance_(other.instance_)
        , bus_(other.bus_)
        , systemLifetime_(std::move(other.systemLifetime_))
        , volume_(other.volume_)
        , pitch_(other.pitch_)
    {
        other.system_ = nullptr;
        other.clip_ = {};
        other.instance_ = {};
    }

    AudioSystem::SoundResource& AudioSystem::SoundResource::operator=(SoundResource&& other) noexcept
    {
        if (this != &other) {
            ReleaseInstance();

            system_ = other.system_;
            clip_ = other.clip_;
            instance_ = other.instance_;
            bus_ = other.bus_;
            systemLifetime_ = std::move(other.systemLifetime_);
            volume_ = other.volume_;
            pitch_ = other.pitch_;

            other.system_ = nullptr;
            other.clip_ = {};
            other.instance_ = {};
        }
        return *this;
    }

    void AudioSystem::SoundResource::ReleaseInstance()
    {
        // AudioSystem が先に破棄されていたら触らない
        // （旧実装はここで生ポインタを辿ってダングリングしていた）
        if (!systemLifetime_.expired()) {
            instance_.Stop();
        }
        instance_ = {};
    }

    bool AudioSystem::SoundResource::Play(bool loop)
    {
        if (!IsValid()) {
            return false;
        }

        // 旧 API の Play は「このリソースを先頭から鳴らし直す」意味だった
        ReleaseInstance();

        instance_ = system_->Play(clip_, {
            .bus = bus_,
            .loop = loop,
            .volume = volume_,
            .pitch = pitch_,
            });
        return instance_.IsValid();
    }

    void AudioSystem::SoundResource::Stop()
    {
        ReleaseInstance();
    }

    void AudioSystem::SoundResource::Pause()
    {
        instance_.Pause();
    }

    void AudioSystem::SoundResource::Resume()
    {
        instance_.Resume();
    }

    void AudioSystem::SoundResource::SetVolume(float volume)
    {
        // 未再生でも値を覚えるので、次の Play() に反映されるし GetVolume() でも読める
        volume_ = std::clamp(volume, 0.0f, 1.0f);
        instance_.SetVolume(volume_);
    }

    float AudioSystem::SoundResource::GetVolume() const
    {
        // 再生中はボイスの現在値（フェード中の値も追える）、未再生なら Set した値
        return instance_.IsValid() ? instance_.GetVolume() : volume_;
    }

    void AudioSystem::SoundResource::SetPitch(float pitch)
    {
        pitch_ = pitch;
        instance_.SetPitch(pitch);
    }

    float AudioSystem::SoundResource::GetPitch() const
    {
        return instance_.IsValid() ? instance_.GetPitch() : pitch_;
    }

    bool AudioSystem::SoundResource::IsPlaying() const
    {
        return instance_.IsPlaying();
    }

    bool AudioSystem::SoundResource::IsPaused() const
    {
        return instance_.IsPaused();
    }

    void AudioSystem::SoundResource::FadeIn(float duration, float targetVolume)
    {
        volume_ = std::clamp(targetVolume, 0.0f, 1.0f);
        instance_.FadeIn(duration, volume_);
    }

    void AudioSystem::SoundResource::FadeOut(float duration, bool stopAfterFade)
    {
        instance_.FadeOut(duration, stopAfterFade);
    }

    bool AudioSystem::SoundResource::IsFading() const
    {
        return instance_.IsFading();
    }

    std::unique_ptr<AudioSystem::SoundResource> AudioSystem::CreateSoundResource(
        const std::string& filename, AudioBus bus)
    {
        SoundClip clip = LoadClip(filename);
        if (!clip.IsValid()) {
            return nullptr;
        }
        return std::make_unique<SoundResource>(this, clip, bus);
    }
}

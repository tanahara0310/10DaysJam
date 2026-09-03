#include "pch.h"
#include "TitleLogoAnimationComponent.h"

#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Camera/Shake/CameraShake.h"
#include "Camera/Shake/CameraShakePresets.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"
#include "Utility/Tween/Tween.h"

using namespace CoreEngine;

void GameComponents::TitleLogoAnimationComponent::Start()
{
    GameObject* owner = GetOwner();
    transform_ = owner ? owner->GetComponent<TransformComponent>() : nullptr;
    if (!owner || !transform_) {
        SetEnabled(false);
        return;
    }

    const WorldTransform& transform = transform_->Get();
    basePosition_ = transform.translate;
    baseRotation_ = transform.rotate;
    baseScale_ = transform.scale;

    // アニメーションの値はタイトルオブジェクト生成時に CVar から読み取る。
    // そのため、ハードコードされた setter 呼び出しをシーン側へ並べず、
    // インスペクターで保存した値を次回のタイトル表示へ反映できる。
    introDuration_ = TitleSceneCVars::IntroDuration.Get();
    introScale_ = TitleSceneCVars::IntroScale.Get();
    dropHeight_ = TitleSceneCVars::DropHeight.Get();
    bobHeight_ = TitleSceneCVars::BobHeight.Get();
    bobDuration_ = TitleSceneCVars::BobDuration.Get();
    rotationAmplitude_ = TitleSceneCVars::RotationAmplitude.Get();
    shakeStrength_ = TitleSceneCVars::ShakeStrength.Get();
    nextBounceIndex_ = 0;

    // シーン側で設定された最終姿勢を保存する。位置は最終地点より上へ移し、
    // そこから落下させる。EaseOutBounce は「速く落ちる → 地面で大きく跳ねる
    // → 小さく2～3回反発して停止」というカーブなので、別の物理システムを
    // 用意しなくてもタイトルロゴらしい着地を作れる。
    Vector3 dropStartPosition = basePosition_;
    dropStartPosition.y += dropHeight_;
    transform_->Get().translate = dropStartPosition;

    // 落下開始時は少し小さくし、わずかに傾ける。落下の勢いが加わることで、
    // 静的に表示されるよりも画面へ入ってくる方向が分かりやすくなる。
    transform_->Get().scale = {
        baseScale_.x * introScale_,
        baseScale_.y * introScale_,
        baseScale_.z * introScale_,
    };

    Vector3 introRotation = baseRotation_;
    introRotation.y -= rotationAmplitude_;
    transform_->Get().rotate = introRotation;

    TweenSequence()
        .Append(
            Tween::To<float>(
                &transform_->Get().translate.y,
                basePosition_.y,
                introDuration_)
                .SetEase(EasingUtil::Type::EaseOutBounce)
                .OnUpdate([this](float progress) { OnIntroProgress(progress); }))
        .Join(
            Tween::ScaleTo(owner, baseScale_, introDuration_)
                .SetEase(EasingUtil::Type::EaseOutBack))
        .Join(
            Tween::RotateTo(owner, baseRotation_, introDuration_)
                .SetEase(EasingUtil::Type::EaseOutCubic))
        .AppendCallback([this] { StartIdleAnimation(); })
        .SetLink(owner)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_logo_intro");
}

void GameComponents::TitleLogoAnimationComponent::OnIntroProgress(float progress)
{
    // EasingUtil::EaseOutBounce の d1=2.75 に対応する接地位置。進捗は Tween の
    // 生の時間進捗なので、フレームレートに関係なく接地を一度ずつ拾える。
    static constexpr float kBounceProgress[] = {
        1.0f / 2.75f,
        2.0f / 2.75f,
        2.5f / 2.75f,
        1.0f,
    };
    static constexpr float kBounceIntensity[] = { 1.0f, 0.55f, 0.3f, 0.18f };

    while (nextBounceIndex_ < 4
        && progress >= kBounceProgress[nextBounceIndex_]) {
        PlayBounceShake(kBounceIntensity[nextBounceIndex_]);
        ++nextBounceIndex_;
    }
}

void GameComponents::TitleLogoAnimationComponent::PlayBounceShake(float intensity)
{
    CoreEngine::CameraShakeParams params = CoreEngine::CameraShakePresets::Landing();
    const float totalIntensity = intensity * shakeStrength_;
    params.positionAmplitude *= totalIntensity;
    params.rotationAmplitude *= totalIntensity;
    CoreEngine::CameraShake::Play(params);
}

void GameComponents::TitleLogoAnimationComponent::StartIdleAnimation()
{
    GameObject* owner = GetOwner();
    if (!owner || !transform_) {
        return;
    }

    // 2 本の Tween が同じ値を同時に書き換えないよう、軸を分けている。
    Tween::To<float>(
        &transform_->Get().translate.y,
        basePosition_.y + bobHeight_,
        bobDuration_)
        .SetEase(EasingUtil::Type::EaseInOutSine)
        .SetLoops(-1, TweenLoop::Yoyo)
        .SetLink(owner)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_logo_bob");

    Tween::To<float>(
        &transform_->Get().rotate.y,
        baseRotation_.y + rotationAmplitude_,
        bobDuration_ * 1.25f)
        .SetEase(EasingUtil::Type::EaseInOutSine)
        .SetLoops(-1, TweenLoop::Yoyo)
        .SetLink(owner)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_logo_sway");
}

void GameComponents::TitleLogoAnimationComponent::OnDestroy()
{
    if (GetOwner()) {
        Tween::KillByLink(GetOwner());
    }
}

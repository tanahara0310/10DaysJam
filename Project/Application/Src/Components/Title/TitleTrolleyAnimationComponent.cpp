#include "pch.h"
#include "TitleTrolleyAnimationComponent.h"

#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"
#include "Utility/Tween/Tween.h"

using namespace CoreEngine;

void GameComponents::TitleTrolleyAnimationComponent::Start()
{
    GameObject* owner = GetOwner();
    transform_ = owner ? owner->GetComponent<TransformComponent>() : nullptr;
    if (!owner || !transform_) {
        SetEnabled(false);
        return;
    }

    basePosition_ = TitleSceneCVars::TrolleyPosition.Get();
    introDuration_ = TitleSceneCVars::TrolleyIntroDuration.Get();
    introOffset_ = TitleSceneCVars::TrolleyIntroOffset.Get();
    bobStart_ = TitleSceneCVars::TrolleyBobStart.Get();
    bobEnd_ = TitleSceneCVars::TrolleyBobEnd.Get();
    bobDuration_ = TitleSceneCVars::TrolleyBobDuration.Get();
    const float introDelay =
        TitleSceneCVars::IntroDuration.Get() + TitleSceneCVars::TrolleyIntroDelay.Get();

    // 最終位置から下へ離した地点を開始位置にする。既定値ではカメラの画面外から
    // タイトルの落下が終わって少し間を置いたあと、EaseOutCubic で減速しながら
    // 所定位置へ収まる。
    Vector3 startPosition = basePosition_;
    startPosition.y -= introOffset_;
    transform_->Get().translate = startPosition;

    Tween::To<float>(
        &transform_->Get().translate.y,
        basePosition_.y,
        introDuration_)
        .SetEase(EasingUtil::Type::EaseOutCubic)
        .SetDelay(introDelay)
        .SetLink(owner)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_trolley_intro")
        .OnComplete([this] { StartIdleAnimation(); });
}

void GameComponents::TitleTrolleyAnimationComponent::StartIdleAnimation()
{
    if (!GetOwner() || !transform_) {
        return;
    }

    // タイトルロゴと同じく、登場後はトロッコを基準にした始点・終点を
    // Vector3 の線形補間で往復する。初期値は X/Z を 0 にしているため、
    // トロッコと、その子である monkey の真上を通る上下移動になる。
    const Vector3 bobStartPosition = basePosition_ + bobStart_;
    const Vector3 bobEndPosition = basePosition_ + bobEnd_;
    transform_->Get().translate = bobStartPosition;

    Tween::To<Vector3>(
        &transform_->Get().translate,
        bobEndPosition,
        bobDuration_)
        .SetEase(EasingUtil::Type::EaseInOutSine)
        .SetLoops(-1, TweenLoop::Yoyo)
        .SetLink(GetOwner())
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_trolley_bob");
}

void GameComponents::TitleTrolleyAnimationComponent::OnDestroy()
{
    if (GetOwner()) {
        Tween::KillByLink(GetOwner());
    }
}

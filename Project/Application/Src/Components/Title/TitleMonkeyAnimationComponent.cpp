#include "pch.h"
#include "TitleMonkeyAnimationComponent.h"

#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"
#include "Utility/Tween/Tween.h"

using namespace CoreEngine;

void GameComponents::TitleMonkeyAnimationComponent::Start()
{
    GameObject* owner = GetOwner();
    transform_ = owner ? owner->GetComponent<TransformComponent>() : nullptr;
    if (!owner || !transform_) {
        SetEnabled(false);
        return;
    }

    basePosition_ = transform_->Get().translate;
    introDuration_ = TitleSceneCVars::MonkeyIntroDuration.Get();
    introOffset_ = TitleSceneCVars::MonkeyIntroOffset.Get();

    // トロッコの到着までは車体の中に沈めておき、到着後に飛び出させる。
    Vector3 startPosition = basePosition_;
    startPosition.y -= introOffset_;
    transform_->Get().translate = startPosition;

    const float introDelay =
        TitleSceneCVars::IntroDuration.Get()
        + TitleSceneCVars::TrolleyIntroDelay.Get()
        + TitleSceneCVars::TrolleyIntroDuration.Get();

    Tween::To<float>(
        &transform_->Get().translate.y,
        basePosition_.y,
        introDuration_)
        .SetEase(EasingUtil::Type::EaseOutBack)
        .SetDelay(introDelay)
        .SetLink(owner)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId("title_monkey_intro");
}

void GameComponents::TitleMonkeyAnimationComponent::OnDestroy()
{
    if (GetOwner()) {
        Tween::KillByLink(GetOwner());
    }
}

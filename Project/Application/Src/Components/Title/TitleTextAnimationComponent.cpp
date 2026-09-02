#include "pch.h"
#include "TitleTextAnimationComponent.h"

#include "GameObject/GameObject.h"
#include "UI/UIText.h"
#include "Utility/Tween/Tween.h"

#include <utility>

using namespace CoreEngine;

void GameComponents::TitleTextAnimationComponent::Start()
{
    auto* text = dynamic_cast<UIText*>(GetOwner());
    if (!text) {
        SetEnabled(false);
        return;
    }

    const Vector2 endPosition = text->GetAnchoredPosition();
    const Vector4 endColor = text->GetColor();

    Vector2 startPosition = endPosition;
    startPosition.y += slideDistance_;

    Vector4 startColor = endColor;
    startColor.w = 0.0f;

    text->SetAnchoredPosition(startPosition);
    text->SetColor(startColor);

    TweenSequence()
        .Append(
            Tween::To<Vector2>(
                startPosition,
                endPosition,
                duration_,
                [text](const Vector2& position) {
                    text->SetAnchoredPosition(position);
                })
                .SetEase(EasingUtil::Type::EaseOutCubic))
        .Join(
            Tween::To<Vector4>(
                startColor,
                endColor,
                duration_,
                [text](const Vector4& color) {
                    text->SetColor(color);
                })
                .SetEase(EasingUtil::Type::EaseOutCubic))
        .SetDelay(delay_)
        .SetLink(text)
        .SetUpdateType(TweenUpdate::Unscaled)
        .SetId(tweenId_);
}

void GameComponents::TitleTextAnimationComponent::OnDestroy()
{
    if (GetOwner()) {
        Tween::KillByLink(GetOwner());
    }
}

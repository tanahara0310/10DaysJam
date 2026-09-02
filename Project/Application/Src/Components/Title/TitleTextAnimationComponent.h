#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector4.h"

#include <string>

namespace GameComponents
{
    /// @brief タイトル画面の UIText のフェード・スライドを制御するコンポーネント
    class TitleTextAnimationComponent final : public CoreEngine::IComponent
    {
    public:
        explicit TitleTextAnimationComponent(
            float delay = 0.0f,
            float slideDistance = 24.0f,
            float duration = 0.45f,
            std::string tweenId = "title_text_intro")
            : delay_(delay),
              slideDistance_(slideDistance),
              duration_(duration),
              tweenId_(std::move(tweenId)) {
        }

        const char* GetTypeName() const override { return "TitleTextAnimation"; }

        void Start() override;
        void OnDestroy() override;

    private:
        float delay_ = 0.0f;
        float slideDistance_ = 24.0f;
        float duration_ = 0.45f;
        std::string tweenId_;
    };
}

#pragma once

#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector4.h"
#include "UI/UIAnchor.h"
#include "Utility/CVar/CVar.h"

#include <functional>
#include <string>

namespace CoreEngine
{
    class UIText;
}

namespace ResultSceneUi
{
    using TextFactory = std::function<CoreEngine::UIText*(
        const std::string& text,
        float fontSize,
        CoreEngine::UIAnchor anchor,
        const CoreEngine::Vector2& anchoredPosition,
        const CoreEngine::Vector4& color,
        const std::string& name)>;

    struct Elements
    {
        CoreEngine::UIText* scoreText = nullptr;
        CoreEngine::UIText* highScoreText = nullptr;
        CoreEngine::UIText* recordUpdatedText = nullptr;
        CoreEngine::UIText* retryButton = nullptr;
        CoreEngine::UIText* titleButton = nullptr;
    };

    // リザルト画面の配置・文字・色は CVar から変更できる。
    extern CoreEngine::CVar<float> TitleFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> TitlePosition;
    extern CoreEngine::CVar<CoreEngine::Vector4> TitleColor;
    extern CoreEngine::CVar<int> TitleSortOrder;
    extern CoreEngine::CVar<float> ScoreFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> ScorePosition;
    extern CoreEngine::CVar<int> ScoreSortOrder;
    extern CoreEngine::CVar<float> HighScoreFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> HighScorePosition;
    extern CoreEngine::CVar<CoreEngine::Vector4> HighScoreColor;
    extern CoreEngine::CVar<int> HighScoreSortOrder;
    extern CoreEngine::CVar<float> RecordUpdatedFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> RecordUpdatedPosition;
    extern CoreEngine::CVar<CoreEngine::Vector4> RecordUpdatedColor;
    extern CoreEngine::CVar<int> RecordUpdatedSortOrder;
    extern CoreEngine::CVar<float> ScoreCountUpDuration;
    extern CoreEngine::CVar<float> RecordUpdatedFadeDuration;
    extern CoreEngine::CVar<float> MenuSlideDuration;
    extern CoreEngine::CVar<float> MenuSlideDistance;
    extern CoreEngine::CVar<float> ButtonFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> ButtonPosition;
    extern CoreEngine::CVar<float> ButtonSpacing;
    extern CoreEngine::CVar<CoreEngine::Vector4> ButtonColor;
    extern CoreEngine::CVar<int> ButtonSortOrder;

    Elements Build(const TextFactory& createText);
    void PlayMenuEntrance(const Elements& elements, bool showRecordUpdated,
        std::function<void()> onFinished);
}

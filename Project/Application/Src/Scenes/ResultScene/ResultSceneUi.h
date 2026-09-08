#pragma once

#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector4.h"
#include "UI/UIAnchor.h"
#include "Utility/CVar/CVar.h"

#include <functional>
#include <string>

namespace CoreEngine
{
    class UIImage;
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

    using ImageFactory = std::function<CoreEngine::UIImage*(
        const std::string& texturePath,
        const std::string& name)>;

    struct Elements
    {
        CoreEngine::UIImage* background = nullptr;
        CoreEngine::UIImage* cinematicTopBar = nullptr;
        CoreEngine::UIImage* cinematicBottomBar = nullptr;
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
    extern CoreEngine::CVar<float> BackgroundPadding;
    extern CoreEngine::CVar<float> BackgroundOpacity;
    extern CoreEngine::CVar<int> BackgroundSortOrder;
    extern CoreEngine::CVar<float> CinematicBarHeight;
    extern CoreEngine::CVar<int> CinematicBarSortOrder;
    extern CoreEngine::CVar<float> ButtonFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> ButtonPosition;
    extern CoreEngine::CVar<float> ButtonSpacing;
    extern CoreEngine::CVar<CoreEngine::Vector4> ButtonColor;
    extern CoreEngine::CVar<int> ButtonSortOrder;

    Elements Build(const TextFactory& createText, const ImageFactory& createImage = {});
}

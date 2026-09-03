#pragma once

#include "Utility/CVar/CVar.h"

/// @brief タイトルシーンを構成する値を CVar としてまとめた名前空間。
/// @details
///  タイトルシーンの配置・落下アニメーション・操作ヒントをコード中の
///  リテラルへ直接書かず、CVarRegistry へ登録する。登録した値は CVarUI から
///  インスペクターで編集でき、既存の CVar 保存機能の対象にもなる。
namespace TitleSceneCVars
{
    // ===== title.obj の最終姿勢 =====
    extern CoreEngine::CVar<CoreEngine::Vector3> Position;
    extern CoreEngine::CVar<CoreEngine::Vector3> Rotation;
    extern CoreEngine::CVar<CoreEngine::Vector3> Scale;

    // ===== ロゴの登場・待機アニメーション =====
    extern CoreEngine::CVar<float> IntroDuration;
    extern CoreEngine::CVar<float> IntroScale;
    extern CoreEngine::CVar<float> DropHeight;
    extern CoreEngine::CVar<float> BobHeight;
    extern CoreEngine::CVar<float> BobDuration;
    extern CoreEngine::CVar<float> RotationAmplitude;
    extern CoreEngine::CVar<float> ShakeStrength;

    // ===== トロッコ登場演出 =====
    extern CoreEngine::CVar<CoreEngine::Vector3> TrolleyPosition;
    extern CoreEngine::CVar<float> TrolleyIntroDelay;
    extern CoreEngine::CVar<float> TrolleyIntroDuration;
    extern CoreEngine::CVar<float> TrolleyIntroOffset;
    extern CoreEngine::CVar<CoreEngine::Vector3> TrolleyBobStart;
    extern CoreEngine::CVar<CoreEngine::Vector3> TrolleyBobEnd;
    extern CoreEngine::CVar<float> TrolleyBobDuration;

    // ===== monkey.obj の配置・登場演出 =====
    extern CoreEngine::CVar<float> MonkeyDistance;
    extern CoreEngine::CVar<float> MonkeyIntroDuration;
    extern CoreEngine::CVar<float> MonkeyIntroOffset;

    // ===== 操作ヒント UI =====
    extern CoreEngine::CVar<float> HintFontSize;
    extern CoreEngine::CVar<CoreEngine::Vector2> HintPosition;
    extern CoreEngine::CVar<CoreEngine::Vector4> HintColor;
    extern CoreEngine::CVar<int> HintSortOrder;
    extern CoreEngine::CVar<float> HintIntroDelay;
    extern CoreEngine::CVar<float> HintSlideDistance;
    extern CoreEngine::CVar<float> HintIntroDuration;

    /// @brief タイトルモデルの配置を表示する CVar 接頭辞。
    inline constexpr const char* kTransformCVarPrefix = "Title.Transform";

    /// @brief タイトルモデルのアニメーションを表示する CVar 接頭辞。
    inline constexpr const char* kAnimationCVarPrefix = "Title.Animation";

    /// @brief タイトルモデルに属さないカメラシェイク設定を表示する CVar 接頭辞。
    inline constexpr const char* kCameraShakeCVarPrefix = "Title.CameraShake";

    /// @brief トロッコ登場演出を表示する CVar 接頭辞。
    inline constexpr const char* kTrolleyCVarPrefix = "Title.Trolley";

    /// @brief monkey.obj の配置・登場演出を表示する CVar 接頭辞。
    inline constexpr const char* kMonkeyCVarPrefix = "Title.Monkey";

    /// @brief 操作ヒントUITextを表示する CVar 接頭辞。
    inline constexpr const char* kHintCVarPrefix = "Title.UI";
}

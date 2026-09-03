#pragma once

#include "Camera/CameraStructs.h"

#include <string>
#include <vector>

/// @file
/// @brief カメラシーケンス（キーフレームで組んだカメラワーク）のデータ型
/// @details エディタ・ランタイムの両方がこの 1 組の型だけを使う。以前は
///          キーフレーム編集モジュール・シーケンス再生モジュール・保存 I/O が
///          それぞれ自前の同名の型を持っており、片方だけ直すと食い違っていた。

namespace CoreEngine
{
    /// @brief シーケンスの既定の置き場所
    namespace CameraSequencePaths {
        /// @brief シーケンス(.json)を探すディレクトリ
        inline constexpr const char* kDirectory = "Application/Assets/Presets/CameraClips/";
    }

    /// @brief ショット間の遷移方式（0:カット / 1:ブレンド）
    /// @note 値は JSON の "transitionType" にそのまま入るため、番号を変えると既存アセットが壊れる。
    enum class CameraSequenceTransitionType {
        Cut = 0,
        Blend = 1
    };

    /// @brief シーケンス内の 1 ショット
    /// @details キーフレーム列の上に重ねる区間の定義。ショットが無くてもシーケンスは成立する。
    struct CameraSequenceShot {
        std::string name;
        float startTime = 0.0f;
        float endTime = 1.0f;
        bool enabled = true;
        CameraSequenceTransitionType transitionType = CameraSequenceTransitionType::Cut;
        float blendDuration = 0.2f;
    };

    /// @brief シーケンス上の 1 キーフレーム（時刻とカメラ姿勢）
    struct CameraSequenceKeyframe {
        float time = 0.0f;
        CameraSnapshot snapshot{};
    };

    /// @brief カメラシーケンス 1 本分のデータ
    struct CameraSequenceAsset {
        /// @brief 保存時に書き込むフォーマットバージョン
        static constexpr const char* kCurrentVersion = "2.0";

        /// @brief タイムライン長の下限（0 秒だと時刻の正規化がゼロ除算になる）
        static constexpr float kMinTimelineLength = 0.1f;

        /// @brief ショットの最小長
        static constexpr float kMinShotDuration = 0.01f;

        std::string version = kCurrentVersion;
        float timelineLength = 10.0f;
        int easingTypeIndex = 0;
        bool shotsEnabled = true;
        std::vector<CameraSequenceKeyframe> keyframes;
        std::vector<CameraSequenceShot> shots;

        /// @brief キーフレームを時刻の昇順へ並べ替える
        /// @details 評価は「隣り合う 2 キーの間を補間する」前提なので、
        ///          キーを足した後・読み込んだ後は必ずこれを通すこと。
        void SortKeyframes();

        /// @brief 各値をタイムライン長の範囲へ収める
        /// @details timelineLength の下限、キー時刻とショット時刻のクランプ、
        ///          ショットの最小長とブレンド長の非負化をまとめて行う。
        void Sanitize();
    };
}

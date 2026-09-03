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

    /// @brief キーから次のキーへ向かう補間の方式
    /// @note 値は JSON の "interpolation" にそのまま入る。番号を変えると既存アセットが壊れる。
    enum class CameraSequenceInterpolation {
        /// @brief 補間しない。次のキーの時刻まで姿勢を固定する（カット割り用）
        Step = 0,
        /// @brief 2 キーを直線で結ぶ
        Linear = 1,
        /// @brief 前後のキーも見て曲線で結ぶ（Catmull-Rom）
        /// @details キーの上は必ず通る。直線だとキーごとに進行方向が折れるのに対し、
        ///          こちらは滑らかに繋がるのでカメラワークらしい動きになる。
        Smooth = 2
    };

    /// @brief キーごとの緩急指定で「シーケンス既定に従う」を表す値
    inline constexpr int kUseSequenceEasing = -1;

    /// @brief カメラの向きの決め方
    /// @note 値は JSON の "aimMode" にそのまま入る。番号を変えると既存アセットが壊れる。
    enum class CameraSequenceAimMode {
        /// @brief キーに保存された回転をそのまま使う
        Euler = 0,
        /// @brief 指定したワールド座標を向く
        LookAtPoint = 1,
        /// @brief 指定した名前のオブジェクトを向く（動く対象を追える）
        LookAtObject = 2
    };

    /// @brief シーケンス上の 1 キーフレーム（時刻とカメラ姿勢）
    struct CameraSequenceKeyframe {
        float time = 0.0f;
        CameraSnapshot snapshot{};

        /// @brief このキーから次のキーへの緩急（kUseSequenceEasing でシーケンス既定）
        /// @details 区間ごとに指定できるので「ここだけゆっくり入る」が作れる。
        int easingTypeIndex = kUseSequenceEasing;

        /// @brief このキーから次のキーへの補間方式
        CameraSequenceInterpolation interpolation = CameraSequenceInterpolation::Linear;

        // ===== 向きの決め方 =====
        // 注視を使うと、位置だけ打てば向きは評価時に計算される。対象を捉えたまま
        // 回り込むショットが、位置と回転を手で合わせずに作れる。

        /// @brief 向きの決め方（既定はキーの回転をそのまま使う）
        CameraSequenceAimMode aimMode = CameraSequenceAimMode::Euler;

        /// @brief LookAtPoint で向くワールド座標
        Vector3 aimPoint = { 0.0f, 0.0f, 0.0f };

        /// @brief LookAtObject で向くオブジェクト名
        std::string aimObjectName;

        /// @brief 注視先に足すオフセット（足元ではなく頭を見る、など）
        Vector3 aimOffset = { 0.0f, 0.0f, 0.0f };

        /// @brief 注視時のロール [ラジアン]（画面を傾ける）
        /// @note Euler のときはこの値ではなく snapshot.rotation.z がロールになる。
        float aimRoll = 0.0f;
    };

    /// @brief カメラシーケンス 1 本分のデータ
    struct CameraSequenceAsset {
        /// @brief 保存時に書き込むフォーマットバージョン
        static constexpr const char* kCurrentVersion = "2.2";

        /// @brief タイムライン長の下限（0 秒だと時刻の正規化がゼロ除算になる）
        static constexpr float kMinTimelineLength = 0.1f;

        /// @brief ショットの最小長
        static constexpr float kMinShotDuration = 0.01f;

        std::string version = kCurrentVersion;
        float timelineLength = 10.0f;
        /// @brief キー側が kUseSequenceEasing のときに使われる既定の緩急
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

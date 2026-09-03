#include "pch.h"
#include "CameraSequenceEvaluator.h"

#include <algorithm>

namespace CoreEngine
{
    namespace
    {
        struct EasingOption {
            const char* label;
            EasingUtil::Type type;
        };

        constexpr EasingOption kEasingOptions[] = {
            { "線形", EasingUtil::Type::Linear },
            { "イーズイン (Quad)", EasingUtil::Type::EaseInQuad },
            { "イーズアウト (Quad)", EasingUtil::Type::EaseOutQuad },
            { "イーズインアウト (Quad)", EasingUtil::Type::EaseInOutQuad },
            { "イーズイン (Cubic)", EasingUtil::Type::EaseInCubic },
            { "イーズアウト (Cubic)", EasingUtil::Type::EaseOutCubic },
            { "イーズインアウト (Cubic)", EasingUtil::Type::EaseInOutCubic },
            { "イーズイン (Quart)", EasingUtil::Type::EaseInQuart },
            { "イーズアウト (Quart)", EasingUtil::Type::EaseOutQuart },
            { "イーズインアウト (Quart)", EasingUtil::Type::EaseInOutQuart },
            { "イーズイン (Sine)", EasingUtil::Type::EaseInSine },
            { "イーズアウト (Sine)", EasingUtil::Type::EaseOutSine },
            { "イーズインアウト (Sine)", EasingUtil::Type::EaseInOutSine },
            { "イーズイン (Expo)", EasingUtil::Type::EaseInExpo },
            { "イーズアウト (Expo)", EasingUtil::Type::EaseOutExpo },
            { "イーズインアウト (Expo)", EasingUtil::Type::EaseInOutExpo }
        };

        constexpr int kEasingOptionCount = static_cast<int>(sizeof(kEasingOptions) / sizeof(kEasingOptions[0]));

        /// @brief 添字が表の範囲内か
        bool IsValidEasingIndex(int index)
        {
            return index >= 0 && index < kEasingOptionCount;
        }
    }

    int CameraSequenceEasing::Count()
    {
        return kEasingOptionCount;
    }

    EasingUtil::Type CameraSequenceEasing::TypeAt(int index)
    {
        return IsValidEasingIndex(index) ? kEasingOptions[index].type : EasingUtil::Type::Linear;
    }

    const char* CameraSequenceEasing::LabelAt(int index)
    {
        return IsValidEasingIndex(index) ? kEasingOptions[index].label : kEasingOptions[0].label;
    }

    bool CameraSequenceEvaluator::Evaluate(const CameraSequenceAsset& asset, float time, CameraSnapshot& outSnapshot)
    {
        if (!EvaluateRaw(asset, time, outSnapshot)) {
            return false;
        }

        // ショット管理有効時は、ショット境界での遷移方式（カット/ブレンド）を適用する。
        if (!asset.shotsEnabled || asset.shots.empty()) {
            return true;
        }

        const float clampedTime = std::clamp(time, 0.0f, asset.timelineLength);
        const int shotIndex = FindShotIndexAt(asset, clampedTime);
        if (shotIndex < 0) {
            return true;
        }

        const CameraSequenceShot& currentShot = asset.shots[shotIndex];
        if (!currentShot.enabled || currentShot.transitionType != CameraSequenceTransitionType::Blend) {
            return true;
        }

        // ブレンド元は「1 つ前の有効なショットの終わりの姿勢」。無ければ繋ぐ相手がいない。
        int previousShotIndex = -1;
        for (int i = shotIndex - 1; i >= 0; --i) {
            if (asset.shots[i].enabled) {
                previousShotIndex = i;
                break;
            }
        }

        if (previousShotIndex < 0) {
            return true;
        }

        const CameraSequenceShot& previousShot = asset.shots[previousShotIndex];
        const float currentShotDuration = (std::max)(currentShot.endTime - currentShot.startTime, 0.0f);
        const float blendDuration = std::clamp(currentShot.blendDuration, 0.0f, currentShotDuration);
        if (blendDuration <= 0.0001f) {
            return true;
        }

        // ブレンドはショット先頭からの一定時間だけ。それ以外は素の評価結果をそのまま使う。
        const float blendStart = currentShot.startTime;
        const float blendEnd = blendStart + blendDuration;
        if (clampedTime < blendStart || clampedTime > blendEnd) {
            return true;
        }

        CameraSnapshot fromSnapshot{};
        if (!EvaluateRaw(asset, previousShot.endTime, fromSnapshot)) {
            return true;
        }

        CameraSnapshot toSnapshot{};
        if (!EvaluateRaw(asset, clampedTime, toSnapshot)) {
            return true;
        }

        const float blendT = std::clamp((clampedTime - blendStart) / blendDuration, 0.0f, 1.0f);
        outSnapshot = Interpolate(fromSnapshot, toSnapshot, blendT,
            CameraSequenceEasing::TypeAt(asset.easingTypeIndex));
        return true;
    }

    bool CameraSequenceEvaluator::EvaluateRaw(const CameraSequenceAsset& asset, float time, CameraSnapshot& outSnapshot)
    {
        const auto& keyframes = asset.keyframes;

        if (keyframes.empty()) {
            return false;
        }

        if (keyframes.size() == 1) {
            outSnapshot = keyframes.front().snapshot;
            return true;
        }

        const float clampedTime = std::clamp(time, 0.0f, asset.timelineLength);

        // 範囲外は端のキーフレームを返す。
        if (clampedTime <= keyframes.front().time) {
            outSnapshot = keyframes.front().snapshot;
            return true;
        }
        if (clampedTime >= keyframes.back().time) {
            outSnapshot = keyframes.back().snapshot;
            return true;
        }

        // 指定時刻が含まれる区間を見つけ、補間結果を返す。
        const EasingUtil::Type easing = CameraSequenceEasing::TypeAt(asset.easingTypeIndex);
        for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const CameraSequenceKeyframe& from = keyframes[i];
            const CameraSequenceKeyframe& to = keyframes[i + 1];

            if (clampedTime >= from.time && clampedTime <= to.time) {
                const float span = to.time - from.time;
                if (span <= 0.0001f) {
                    outSnapshot = from.snapshot;
                    return true;
                }

                const float t = (clampedTime - from.time) / span;
                outSnapshot = Interpolate(from.snapshot, to.snapshot, t, easing);
                return true;
            }
        }

        outSnapshot = keyframes.back().snapshot;
        return true;
    }

    // 2 つのスナップショットを補間する。回転だけは LerpAngle で最短経路を通す
    // （素直に線形補間すると 359°→1° で 1 周してしまう）
    CameraSnapshot CameraSequenceEvaluator::Interpolate(const CameraSnapshot& from, const CameraSnapshot& to,
        float t, EasingUtil::Type easing)
    {
        CameraSnapshot result{};

        result.position = EasingUtil::LerpVector3(from.position, to.position, t, easing);
        result.rotation = Vector3(
            EasingUtil::LerpAngle(from.rotation.x, to.rotation.x, t, easing),
            EasingUtil::LerpAngle(from.rotation.y, to.rotation.y, t, easing),
            EasingUtil::LerpAngle(from.rotation.z, to.rotation.z, t, easing));
        result.scale = EasingUtil::LerpVector3(from.scale, to.scale, t, easing);

        // 投影方式は補間できないので、始点のものを引き継ぐ。
        result.parameters.projectionType = from.parameters.projectionType;
        result.parameters.fov = EasingUtil::Lerp(from.parameters.fov, to.parameters.fov, t, easing);
        result.parameters.nearClip = EasingUtil::Lerp(from.parameters.nearClip, to.parameters.nearClip, t, easing);
        result.parameters.farClip = EasingUtil::Lerp(from.parameters.farClip, to.parameters.farClip, t, easing);
        result.parameters.aspectRatio = EasingUtil::Lerp(from.parameters.aspectRatio, to.parameters.aspectRatio, t, easing);
        return result;
    }

    int CameraSequenceEvaluator::FindShotIndexAt(const CameraSequenceAsset& asset, float time)
    {
        for (int i = 0; i < static_cast<int>(asset.shots.size()); ++i) {
            const CameraSequenceShot& shot = asset.shots[i];
            if (!shot.enabled) {
                continue;
            }

            if (time >= shot.startTime && time <= shot.endTime) {
                return i;
            }
        }
        return -1;
    }
}

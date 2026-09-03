#include "pch.h"
#include "CameraSequence.h"

#include <algorithm>

namespace CoreEngine
{
    void CameraSequenceAsset::SortKeyframes()
    {
        std::sort(keyframes.begin(), keyframes.end(),
            [](const CameraSequenceKeyframe& lhs, const CameraSequenceKeyframe& rhs) {
                return lhs.time < rhs.time;
            });
    }

    void CameraSequenceAsset::Sanitize()
    {
        if (timelineLength < kMinTimelineLength) {
            timelineLength = kMinTimelineLength;
        }

        for (auto& key : keyframes) {
            key.time = std::clamp(key.time, 0.0f, timelineLength);
        }

        for (auto& shot : shots) {
            shot.startTime = std::clamp(shot.startTime, 0.0f, timelineLength);
            shot.endTime = std::clamp(shot.endTime, 0.0f, timelineLength);

            // 終了が開始以下だと区間の長さがゼロ以下になり、
            // 「この時刻はどのショットか」の判定が破綻する。
            if (shot.endTime <= shot.startTime) {
                shot.endTime = std::clamp(shot.startTime + kMinShotDuration, kMinShotDuration, timelineLength);
            }

            if (shot.blendDuration < 0.0f) {
                shot.blendDuration = 0.0f;
            }
        }
    }
}

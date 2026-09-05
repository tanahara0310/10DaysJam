#pragma once

#include <algorithm>

namespace GameComponents
{
    /// @brief シーンをまたいでゲーム結果を受け渡すための共有データ。
    /// @details GameScene 開始時にリセットし、終了時に値を確定する。
    class GameResultData final
    {
    public:
        static void Reset()
        {
            travelDistance_ = 0.0f;
        }

        static void SetTravelDistance(float distance)
        {
            travelDistance_ = std::max(0.0f, distance);
        }

        /// @brief 列車が進んだワールド距離を取得する。
        /// @note ResultScene からこの getter を呼ぶと、直前のプレイ結果を取得できる。
        static float GetTravelDistance()
        {
            return travelDistance_;
        }

    private:
        static inline float travelDistance_ = 0.0f;
    };
}

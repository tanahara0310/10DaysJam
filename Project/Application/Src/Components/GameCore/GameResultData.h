#pragma once

#include <cstdint>

namespace GameComponents
{
    /// @brief シーンをまたいでゲーム結果を受け渡すための共有データ。
    /// @details GameScene 開始時にリセットし、終了時に値を確定する。
    class GameResultData final
    {
    public:
        static void Reset()
        {
            horizontalProgressBlocks_ = 0;
        }

        static void SetHorizontalProgressBlocks(uint32_t blocks)
        {
            horizontalProgressBlocks_ = blocks;
        }

        /// @brief 列車が開始位置からX正方向へ進んだ最大ブロック数を取得する。
        /// @note ResultScene からこの getter を呼ぶと、直前のプレイ結果を取得できる。
        static uint32_t GetHorizontalProgressBlocks()
        {
            return horizontalProgressBlocks_;
        }

    private:
        static inline uint32_t horizontalProgressBlocks_ = 0;
    };
}

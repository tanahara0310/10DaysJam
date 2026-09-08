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

        /// @brief 現在の結果をハイスコアへ反映する。
        /// @return 今回の結果でハイスコアを更新した場合は true。
        /// @note ハイスコアは GameScene の開始時にもリセットせず、アプリ実行中は保持する。
        static bool SubmitScore(uint32_t score)
        {
            if (score <= highScoreBlocks_) {
                return false;
            }

            highScoreBlocks_ = score;
            return true;
        }

        /// @brief 現在のハイスコアを取得する。
        static uint32_t GetHighScore()
        {
            return highScoreBlocks_;
        }

    private:
        static inline uint32_t horizontalProgressBlocks_ = 0;
        static inline uint32_t highScoreBlocks_ = 0;
    };
}

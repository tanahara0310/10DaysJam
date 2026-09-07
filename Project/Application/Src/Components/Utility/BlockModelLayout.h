#pragma once

namespace GameComponents::BlockModelLayout
{
    // 各モデルは XZ 幅 1.6 の共通ブロックを基準に、底面を Y=0 として制作されている。
    // 個々の外接箱に合わせて拡縮せず、制作時の比率を保って1マスへ変換する。
    inline constexpr float kModelBlockSize = 1.6f;
    inline constexpr float kRailModelHeight = 0.2f;
    inline constexpr float kBridgeModelHeight = 0.2f;
    inline constexpr float kStationModelHeight = 3.0f;

    constexpr float GetScale(float gridSize) {
        return gridSize / kModelBlockSize;
    }

    constexpr float GetGroundHeight(float gridSize) {
        return -gridSize * 0.5f;
    }

    constexpr float GetSurfaceHeight(float gridSize) {
        return GetGroundHeight(gridSize) + kModelBlockSize * GetScale(gridSize);
    }

    constexpr float GetBridgeHeight(float gridSize) {
        // 橋の上面と地面の上面を揃え、どちらでも同じ高さにレールを置く。
        return GetSurfaceHeight(gridSize) - kBridgeModelHeight * GetScale(gridSize);
    }

    constexpr float GetRailTopHeight(float gridSize) {
        return GetSurfaceHeight(gridSize) + kRailModelHeight * GetScale(gridSize);
    }
}

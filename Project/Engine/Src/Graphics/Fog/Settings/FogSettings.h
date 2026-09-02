#pragma once

#include "Math/Vector/Vector3.h"

namespace CoreEngine
{
    /// @brief フォグの見た目パラメータ（単位はメートル・無次元）
    /// @details 値の実体は FogCVars が持つ。既定値もそちらにあるため、ここでは初期化しない。
    ///          FogManager::Update が毎フレーム LoadInto でここへ取り込む。
    ///
    /// 密度モデルは rho(y) = density * exp(-heightFalloff * (y - heightRefM))。
    /// heightFalloff = 0 なら高さ非依存の指数距離フォグ、大きくすれば地表に溜まる霧になる。
    /// 数式の実体は Assets/Shaders/Include/Common/Fog.hlsli。
    struct FogSettings {
        bool    enabled;         ///< フォグ合成を行うか
        Vector3 color;           ///< フォグ色（リニア HDR。トーンマップ前の値）
        float   density;         ///< heightRefM における消散係数 [1/m]
        float   heightFalloff;   ///< 高さ方向の減衰率 [1/m]。0 で高さ非依存
        float   heightRefM;      ///< density を与える基準高度 [m]
        float   startDistanceM;  ///< フォグが効き始めるカメラからの距離 [m]
        float   maxOpacity;      ///< 最大濃度 [0,1]。1 未満なら遠景が消えきらない
        float   skyDistanceM;    ///< 背景ピクセルのレイ長 [m]。0 以下ならファークリップを使う
        bool    applyToSky;      ///< 背景（深度 far）にもフォグを掛けるか
    };
}

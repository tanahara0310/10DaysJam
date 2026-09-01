#pragma once

#include "Math/Vector/Vector3.h"

namespace ShootingSample
{
    /// @brief このシーンで起きる出来事の一覧
    /// @details
    ///  イベントは継承もマクロ登録も要らない、ただの構造体。
    ///  1 か所にまとめてあるので、このファイルがそのまま
    ///  「このゲームで何が起きるか」の目次になる。
    ///
    ///  発行側（弾・自機）は誰が聞いているかを知らず、
    ///  購読側（HUD）は誰が投げたかを知らない。
    ///  そのため HUD を消しても弾のコードは 1 行も変わらないし、
    ///  逆に SE やエフェクトを足すときも既存のコードを触らなくていい。

    /// @brief 敵が倒された
    struct EnemyDiedEvent {
        CoreEngine::Vector3 position{}; ///< 倒された場所（エフェクトの発生位置に使う）
        int score = 0;                  ///< 加算する基礎点（コンボ倍率は購読側が掛ける）
    };

    /// @brief 敵が撃ち漏らされて画面外へ抜けた
    struct EnemyEscapedEvent {
    };

    /// @brief 自機が被弾した
    struct PlayerDamagedEvent {
        int remainingLife = 0; ///< 残り機数（0 でゲームオーバー）
    };
}

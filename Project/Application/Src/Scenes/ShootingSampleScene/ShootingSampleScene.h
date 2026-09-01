#pragma once

#include "Scene/BaseScene.h"

namespace ShootingSample
{
    class ShipControllerComponent;

    /// @brief 弾を撃って敵を倒すサンプルシーン。
    /// @details
    ///  生成のみを行い、ゲーム処理は各コンポーネントが持つ。
    ///  スコア・コンボ・残機の連動は EventBus 経由なので、
    ///  弾・敵・自機と HUD は互いを知らない。
    class ShootingSampleScene : public CoreEngine::BaseScene {
    public:
        void OnInitialize() override;

    private:
        /// @brief スコア・コンボ・残機の表示を組み立てる
        /// @param ship 初期残機の取得元（nullptr でも既定値で組む）
        void BuildHud(ShipControllerComponent* ship);
    };
}

#pragma once

#include "Scene/Feature/ISceneFeature.h"

#include <memory>

namespace GameComponents
{
    /// @brief レール先頭の「次に伸ばせる向き」を床の矢印で見せる Feature を作る
    /// @details 登録は `AddFeature(GameComponents::CreateRailDirectionGuideFeature())` の 1 行でよい。
    ///          シーンにいる RailPathComponent / MapGeneratorComponent / HungerComponent を
    ///          自分で探して繋ぐので、シーン側からコンポーネントを渡す必要はない。
    /// @note 出すのは 3D テキスト（Text3DObject）で、床と平行に寝かせた矢印を
    ///       先頭マスの上下左右へ 1 つずつ置く。シリアライズ対象から外してあるため
    ///       シーンの JSON には残らない。
    ///       見た目の調整は CVar `Game.RailGuide.*`（インスペクターの「ゲーム設定」）で行う。
    std::unique_ptr<CoreEngine::ISceneFeature> CreateRailDirectionGuideFeature();
}

#pragma once

#include <memory>

namespace CoreEngine {
    class ISceneFeature;
}

namespace GameComponents
{
    /// @brief ステージのブロックより下を雲で埋める Feature を作る
    /// @details GameScene::OnInitialize() から `AddFeature(CreateSkyFogFeature())` で登録する。
    ///          シーンにいる間だけエンジンの高さフォグ（r.Fog.*）を雲の設定に差し替え、
    ///          シーンを抜けるときに元へ戻す（タイトル・リザルトへ持ち出さないため）。
    /// @note 調整値は SkyFogFeature.cpp のファイルスコープにある `Game.Fog.*` の CVar 群。
    ///       CVars.json へ自動保存され、インスペクターの「ゲーム設定」から編集できる。
    std::unique_ptr<CoreEngine::ISceneFeature> CreateSkyFogFeature();
}

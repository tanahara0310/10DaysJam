#pragma once

#include <memory>

namespace CoreEngine {
    class ISceneFeature;
}

namespace GameComponents
{
    /// @brief ゲームシーンの突入演出と、500m の目標提示をまとめて受け持つ Feature を作る
    ///
    /// @details GameScene::OnInitialize() から `AddFeature(CreateGameEntranceFeature())` の
    ///          1 行で登録する。列車・トーンマップ・距離目盛りは Feature が自分で探して繋ぐので、
    ///          シーン側から渡すものは無い。
    ///
    ///          **突入演出（雲海ブレイク → もくひょう看板 → つなげ！！）**
    ///          1. 雲の中から始まる。高さフォグの雲海を画面の上まで持ち上げた状態で開幕し、
    ///             それを島の下まで沈めることで「雲を抜けて降りる」を作る。
    ///          2. カメラは真上のリグ（`Entrance_Sky`）から始まり、通常の `GamePlay` リグへ
    ///             ブレンドして降りる。構図は Presets/CameraRigs/ の json 側が持つ。
    ///          3. ツタで吊るした木の看板が降りてきて、目標「500ｍ」を提示する。
    ///          4. 締めに「つなげ！！」を叩き込んで演出を終える。
    ///
    ///          既定の尺はおよそ 4 秒（白幕が晴れる 1.4 → 看板 1.5〜3.5 →
    ///          「つなげ！！」3.2 → HUD が出揃って操作解禁 4.0）。
    ///
    ///          演出のあいだはプレイヤーの操作を止める。止めるのはレールカーソル
    ///          （`RailBuilderComponent::SetInputLocked`）の**入力だけ**で、矢印の回転と
    ///          脈打ちはそのまま回り続ける。返すのは「つなげ！！」と HUD が出揃って
    ///          演出が終わった時点。列車は最初のレールが敷かれるまで発車しないので、
    ///          これで演出中はゲームが進まない。
    ///
    ///          **目標**
    ///          目標は `Game.Goal.Meters`（既定 500m）の 1 つだけ。列車がそこを越えると、
    ///          地面の距離目盛り（`MapViewComponent` が置く `DistanceMarker_*`）のうち
    ///          目標の目盛りが白く光ってから達成の色へ落ち着く。**次の目標は作らない**ので、
    ///          看板が降りてくるのは開幕の 1 回きり。
    ///          目盛りの色は「未達＝赤（脈打つ）」「達成＝緑」「それ以外＝白のまま」。
    ///
    /// @note 距離の単位は地面の目盛りと同じで、ワールド座標 X がそのままメートル。
    ///       列車の絶対位置で判定するので、看板の数字と足元の目盛りの数字が必ず一致する。
    ///
    /// @note 調整値はすべて CVar が持つ（`Game.Entrance.*` と `Game.Goal.*`、
    ///       看板の見た目は `Game.ObjectiveSign.*`）。インスペクターの「ゲーム設定」から
    ///       編集でき、CVars.json へ自動保存される。`Game.Entrance.Enabled` を切ると
    ///       雲海とカメラ演出だけを飛ばし、看板と目標の進行はそのまま残る。
    std::unique_ptr<CoreEngine::ISceneFeature> CreateGameEntranceFeature();
}

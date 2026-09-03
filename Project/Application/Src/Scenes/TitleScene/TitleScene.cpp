#include "pch.h"
#include "TitleScene.h"

#include "Scenes/TitleScene/TitleSceneModelSetup.h"
#include "Scenes/TitleScene/TitleHintText.h"
#include "Scenes/TitleScene/TitleSceneUi.h"
#include "EngineSystem/EngineSystem.h"
#include "Input/InputManager.h"
#include "Input/InputQuery.h"
#include "Scene/SceneManager.h"
#include "UI/UIText.h"

using namespace CoreEngine;

void TitleScene::TitleScene::OnInitialize()
{
    SetSceneName("TitleScene");

    // タイトルシーンでもエンジン標準の地面を使う。
    //
    // BaseScene はシーン初期化後に GroundFeature::PostSceneInitialize() を呼び、
    // ここが有効なら DefaultGround を自動生成する。以前ここで false を渡して
    // いたため、タイトルシーンだけ地面が生成されず、床と同じ高さに描かれる
    // グリッドも確認しにくい状態になっていた。
    SetDefaultGroundEnabled(true);

    // title.json / _scene.json に依存せず、必要なオブジェクトをコードで構築する。
    // 生成処理の詳細は専用ファイルへ分離し、このクラスは「シーンを組み立てる順番」
    // だけを担当する。
    TitleSceneModel::Build([this](const std::string& name) {
        return CreateObject(name);
    });

    // UI の見た目・アニメーション設定は TitleSceneUi に閉じ込める。
    // シーン本体は、現在の入力デバイスを初期表示へ渡すだけにする。
    if (engine_) {
        if (auto* inputManager = engine_->GetService<InputManager>()) {
            gamepadConnected_ = inputManager->GetQuery().IsGamepadConnected();
        }
    }

    const TitleSceneUi::Elements ui = TitleSceneUi::Build(
        [this](const std::string& text,
               float fontSize,
               UIAnchor anchor,
               const Vector2& position,
               const Vector4& color,
               const std::string& name) -> UIText* {
            // StartHint はコードで初期生成するが、UIText のインスペクターで
            // 編集したフォント・サイズ・位置をシーンJSONへ保存できるようにする。
            // 専用UITextを使うことで、ヒント用CVarもStartHint自身に表示する。
            if (name == "StartHint") {
                auto* hint = CreateObject<TitleSceneUi::TitleHintText>();
                if (!hint) {
                    return static_cast<UIText*>(nullptr);
                }

                hint->SetName(name);
                hint->SetText(text);
                hint->SetFontSize(fontSize);
                hint->SetAnchor(anchor);
                hint->SetAnchoredPosition(position);
                hint->SetColor(color);
                return hint;
            }

            return CreateText(text, fontSize, anchor, position, color, name);
        },
        gamepadConnected_);

    startHint_ = ui.startHint;
}

void TitleScene::TitleScene::OnUpdate()
{
    auto* inputManager = engine_ ? engine_->GetService<InputManager>() : nullptr;
    if (!inputManager || startRequested_) {
        return;
    }

    InputQuery& input = inputManager->GetQuery();

    // 接続状態が途中で変わった場合も、表示文言をその場で切り替える。
    // XInput コントローラの抜き差しをしても、実際に受け付ける入力と画面表示が
    // 食い違わないようにする。
    const bool gamepadConnected = input.IsGamepadConnected();
    if (gamepadConnected != gamepadConnected_) {
        gamepadConnected_ = gamepadConnected;
        TitleSceneUi::UpdateStartPrompt(startHint_, gamepadConnected_);
    }

    // 入力設定に登録されている UIConfirm を使う。現在の既定設定では、ゲームパッド
    // のAボタンとキーボードのSPACEキーが登録されているため、キーコンフィグを
    // 変更した場合もタイトル画面だけ判定が取り残されない。
    if (input.IsActionTriggered(InputAction::UIConfirm)) {
        StartGame();
        return;
    }
}

void TitleScene::TitleScene::StartGame()
{
    if (startRequested_ || !sceneManager_) {
        return;
    }

    startRequested_ = true;
    sceneManager_->ChangeScene("GameScene");
}

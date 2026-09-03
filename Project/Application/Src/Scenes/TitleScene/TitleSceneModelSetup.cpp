#include "pch.h"
#include "TitleSceneModelSetup.h"

#include "Components/Title/TitleLogoAnimationComponent.h"
#include "Components/Title/TitleSceneSettingsComponent.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Scenes/TitleScene/TitleSceneCVars.h"

namespace TitleSceneModel
{
    void Build(const ObjectFactory& createObject)
    {
        if (!createObject) {
            return;
        }

        // シーン JSON の復元に頼らず、まず空の GameObject を作る。
        // title.obj は MeshRendererComponent が Awake() で読み込むため、
        // ここで指定するパスは Application/Assets/Models/Title/title.obj を
        // 実行時のアセットルートから見た相対パスとして解決される。
        CoreEngine::GameObject* titleObject = createObject("title");
        if (!titleObject) {
            return;
        }

        // このオブジェクトは TitleSceneModel::Build() が毎回生成するため、
        // シーンJSON側の同名オブジェクトで上書き・二重復元されないようにする。
        titleObject->SetSerializeEnabled(false);

        auto* transform = titleObject->AddComponent<CoreEngine::TransformComponent>();
        if (!transform) {
            return;
        }

        transform->Get().translate = TitleSceneCVars::Position.Get();
        transform->Get().rotate = TitleSceneCVars::Rotation.Get();
        transform->Get().scale = TitleSceneCVars::Scale.Get();

        // OBJ の読み込み・マテリアル設定は既存の MeshRendererComponent に任せる。
        // この分離により、モデル形式や読み込み処理を変更してもシーンの UI 実装へ
        // 影響が伝播しない。
        titleObject->AddComponent<CoreEngine::MeshRendererComponent>("title.obj");

        // ロゴの登場・上下の浮遊・左右の揺れは専用コンポーネントへ委譲する。
        // シーンはコンポーネントを追加するだけで、毎フレームの Tween 制御を持たない。
        titleObject->AddComponent<GameComponents::TitleSceneSettingsComponent>();
        auto* animation =
            titleObject->AddComponent<GameComponents::TitleLogoAnimationComponent>();
        if (!animation) {
            return;
        }

    }
}

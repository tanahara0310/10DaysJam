#include "pch.h"
#include "ResultScene.h"

#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "EngineSystem/EngineSystem.h"
#include "Text/FontManager.h"
#include "UI/UIText.h"
#include "Utility/Logger/Logger.h"

void ResultScene::ResultScene::OnInitialize() {
    // ========== シーンの設定 ==========
    SetSceneName("ResultScene");
    SetDefaultGroundEnabled(true);
    // ========== オブジェクトの生成 ==========
}

void ResultScene::ResultScene::OnUpdate() {
    // シーンの更新処理をここに記述
}

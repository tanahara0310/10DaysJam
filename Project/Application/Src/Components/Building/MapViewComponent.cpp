#include "pch.h"
#include "MapViewComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "MapGeneratorComponent.h"
#include "Components/Utility/ModelRenderPoolComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>

using namespace CoreEngine;

void GameComponents::MapViewComponent::Start() {
}

void GameComponents::MapViewComponent::Update() {
    // マップジェネレーターとグラウンドレンダープールが有効か確認する
    if (mapGenerator_ == nullptr || groundRenderPool_ == nullptr) {
        return;
    }

    // 描画する範囲を決定する
    size_t startX = (mapViewCenterX_ > viewDistanceX_) ? (mapViewCenterX_ - viewDistanceX_) : 0;
    size_t endX = mapViewCenterX_ + viewDistanceX_;

    // マップチップの2D配列を取得する
    const auto& mapChips = mapGenerator_->GetMapChips();
    // 描画範囲内のマップチップを描画する
    for (size_t x = startX; x < endX && x < mapChips.size(); ++x) {
        for (size_t z = 0; z < mapChips[x].size(); ++z) {
            const auto& chipType = mapChips[x][z];
            // チップの種類に応じて描画する
            if (chipType == MapChipType::Ground) {
                // グラウンドチップの表示
                groundRenderPool_->Draw({ x * gridSize_, 0.0f, z * gridSize_ });
            }
            
        }
    }
}

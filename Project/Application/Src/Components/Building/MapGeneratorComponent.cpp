#include "pch.h"
#include "MapGeneratorComponent.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputAction.h"
#include "Input/InputManager.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>

using namespace CoreEngine;

GameComponents::MapGeneratorComponent::MapGeneratorComponent(uint32_t mapSizeZ, uint32_t startGenerateX)
    : mapSizeZ_(mapSizeZ) {
    // 初期マップを生成する
    AddMapChips(startGenerateX);
}

void GameComponents::MapGeneratorComponent::Start() {

}

void GameComponents::MapGeneratorComponent::Update() {
    
}

void GameComponents::MapGeneratorComponent::CreateToX(std::size_t xCount) {
    if (xCount <= mapChips_.size()) {
        return;
    }
    AddMapChips(xCount - mapChips_.size());
}

void GameComponents::MapGeneratorComponent::AddMapChips(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        std::vector<MapChipType> newRow(mapSizeZ_, MapChipType::Ground);
        mapChips_.push_back(newRow);
    }
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "MapGeneratorComponent: AddMapChips: {} 行追加しました。現在の行数: {}", count, mapChips_.size());
}

const std::vector<std::vector<GameComponents::MapChipType>>& GameComponents::MapGeneratorComponent::GetMapChips() {
    return mapChips_;
}

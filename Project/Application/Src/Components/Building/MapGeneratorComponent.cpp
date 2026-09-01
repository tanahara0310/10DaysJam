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
        // ランダムに穴を配置する（10%の確率で穴を配置）
        for (std::size_t z = 0; z < mapSizeZ_; ++z) {
            if (rand() % 10 == 0) { // 10%の確率
                mapChips_.back()[z] = MapChipType::Void;
            }
        }
        // ランダムに資源を配置する（5%の確率で資源を配置）
        for (std::size_t z = 0; z < mapSizeZ_; ++z) {
            if (rand() % 20 == 0) { // 5%の確率
                mapChips_.back()[z] = MapChipType::Resource;
            }
        }
        // 駅を建設する間隔で駅チップを配置する
        if ((mapChips_.size() - 1) % stationBuildInterval_ == 0) {
            std::size_t stationZ = rand() % mapSizeZ_; // ランダムなZ座標に駅を配置
            mapChips_.back()[stationZ] = MapChipType::Station;
        }
    }
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "MapGeneratorComponent: AddMapChips: {} 行追加しました。現在の行数: {}", count, mapChips_.size());
}

GameComponents::MapChipType GameComponents::MapGeneratorComponent::GetMapChip(
    std::size_t x, std::size_t z) const {
    if (x >= mapChips_.size() || z >= mapSizeZ_) {
        return MapChipType::Void;
    }
    return mapChips_[x][z];
}

bool GameComponents::MapGeneratorComponent::SetMapChip(
    std::size_t x, std::size_t z, MapChipType type) {
    if (z >= mapSizeZ_) {
        return false;
    }
    CreateToX(x + 1);
    mapChips_[x][z] = type;
    return true;
}

const std::vector<std::vector<GameComponents::MapChipType>>& GameComponents::MapGeneratorComponent::GetMapChips() const {
    return mapChips_;
}

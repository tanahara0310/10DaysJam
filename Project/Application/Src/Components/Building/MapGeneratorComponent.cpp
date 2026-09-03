#include "pch.h"
#include "MapGeneratorComponent.h"

#include "Utility/Logger/Logger.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utility>

using namespace CoreEngine;

namespace {
    // 地形セルは単一行。引用符で囲まれたセル・エスケープされた引用符も扱う。
    std::vector<std::string> SplitCsvRow(const std::string& line) {
        std::vector<std::string> cells;
        std::string cell;
        bool quoted = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char ch = line[i];
            if (ch == '"') {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    quoted = !quoted;
                }
            } else if (ch == ',' && !quoted) {
                cells.push_back(std::move(cell));
                cell.clear();
            } else {
                cell += ch;
            }
        }
        // 閉じていない引用符は不正セルとしてVoidにする。
        cells.push_back(quoted ? "<invalid>" : std::move(cell));
        return cells;
    }

    GameComponents::MapChipType ParseChip(std::string cell, std::size_t& invalidCount) {
        using GameComponents::MapChipType;
        const auto first = cell.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return MapChipType::Void;
        }
        cell = cell.substr(first, cell.find_last_not_of(" \t\r\n") - first + 1);
        std::transform(cell.begin(), cell.end(), cell.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        // 数値IDはCSVの仕様として明示し、enumの並び順には依存させない。
        if (cell == "0" || cell == "void") return MapChipType::Void;
        if (cell == "1" || cell == "water") return MapChipType::Water;
        if (cell == "2" || cell == "ground") return MapChipType::Ground;
        if (cell == "3" || cell == "station") return MapChipType::Station;
        if (cell == "4" || cell == "resource") return MapChipType::Resource;
        ++invalidCount;
        return MapChipType::Void;
    }
}

GameComponents::MapGeneratorComponent::MapGeneratorComponent(
    uint32_t mapSizeZ, uint32_t startGenerateX, MapGenerationSettings settings)
    : mapSizeZ_(mapSizeZ), settings_(std::move(settings)) {
    if (settings_.mode == MapGenerationMode::FixedCsv) {
        fixedCsv_ = LoadCsv(settings_.fixedCsvPath);
    } else if (settings_.mode == MapGenerationMode::RandomCsvPool) {
        settings_.csvChunkSizeX = std::max<std::size_t>(1, settings_.csvChunkSizeX);
        csvRandom_.seed(settings_.randomSeed ? *settings_.randomSeed : std::random_device{}());
        LoadCsvPools();
    }
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
    if (settings_.mode == MapGenerationMode::Procedural) {
        AddProceduralMapChips(count);
    } else {
        AddCsvMapChips(count);
    }
    Logger::GetInstance().Infof(
        LogCategory::Game,
        "MapGeneratorComponent: AddMapChips: {} 行追加しました。現在の行数: {}", count, mapChips_.size());
}

void GameComponents::MapGeneratorComponent::LoadCsvPools() {
    auto definitions = settings_.csvPools;
    if (definitions.empty() && !settings_.csvPoolPaths.empty()) {
        definitions.push_back({ "Default", settings_.csvPoolPaths });
    }
    for (const auto& definition : definitions) {
        if (definition.name.empty() || std::any_of(csvPools_.begin(), csvPools_.end(),
            [&](const LoadedCsvPool& pool) { return pool.name == definition.name; })) {
            Logger::GetInstance().Warnf(LogCategory::Game,
                "MapGenerator: 空または重複したプール名を無視しました: {}", definition.name);
            continue;
        }
        LoadedCsvPool pool{ definition.name, {} };
        for (const auto& path : definition.paths) {
            pool.maps.push_back(LoadCsv(path, settings_.csvChunkSizeX));
        }
        csvPools_.push_back(std::move(pool));
    }

    if (!settings_.initialCsvPoolName.empty()) {
        SelectCsvPool(settings_.initialCsvPoolName);
    } else if (!csvPools_.empty()) {
        selectedCsvPoolIndex_ = 0;
    }
    if (!selectedCsvPoolIndex_) {
        Logger::GetInstance().Warnf(LogCategory::Game,
            "MapGenerator: 有効なCSVプールが未選択です。Voidで生成します");
    }
}

bool GameComponents::MapGeneratorComponent::SelectCsvPool(const std::string& name) {
    if (settings_.mode != MapGenerationMode::RandomCsvPool) {
        return false;
    }
    for (std::size_t i = 0; i < csvPools_.size(); ++i) {
        if (csvPools_[i].name == name) {
            selectedCsvPoolIndex_ = i;
            // activeCsvPoolIndex_とactiveCsvColumn_は維持し、区画の途中では混ぜない。
            return true;
        }
    }
    Logger::GetInstance().Warnf(LogCategory::Game,
        "MapGenerator: CSVプールが見つかりません: {}", name);
    return false;
}

std::string GameComponents::MapGeneratorComponent::GetSelectedCsvPoolName() const {
    return selectedCsvPoolIndex_ ? csvPools_[*selectedCsvPoolIndex_].name : "";
}

std::string GameComponents::MapGeneratorComponent::GetActiveCsvPoolName() const {
    return activeCsvPoolIndex_ ? csvPools_[*activeCsvPoolIndex_].name : "";
}

std::vector<std::string> GameComponents::MapGeneratorComponent::GetCsvPoolNames() const {
    std::vector<std::string> names;
    for (const auto& pool : csvPools_) {
        names.push_back(pool.name);
    }
    return names;
}

#ifdef USE_IMGUI
bool GameComponents::MapGeneratorComponent::DrawInspector() {
    if (settings_.mode != MapGenerationMode::RandomCsvPool) {
        ImGui::TextUnformatted("プール切替はRandomCsvPool方式で使用できます。");
        return false;
    }
    bool changed = false;
    const std::string selectedName = GetSelectedCsvPoolName();
    if (ImGui::BeginCombo("エリアプール", selectedName.empty() ? "未選択" : selectedName.c_str())) {
        for (const auto& pool : csvPools_) {
            const bool selected = pool.name == selectedName;
            if (ImGui::Selectable(pool.name.c_str(), selected) && !selected) {
                changed = SelectCsvPool(pool.name);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    const std::size_t nextBoundary = mapChips_.size() +
        (activeCsvColumn_ == 0 ? 0 : settings_.csvChunkSizeX - activeCsvColumn_);
    ImGui::Text("次の区画開始X: %zu", nextBoundary);
    ImGui::TextWrapped("生成済みの地形は保持し、次の未生成区画から切り替わります。選択はこのプレイ中のみ有効です。");
    return changed;
}
#endif

void GameComponents::MapGeneratorComponent::AddProceduralMapChips(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        std::vector<MapChipType> newRow(mapSizeZ_, MapChipType::Ground);
        mapChips_.push_back(newRow);
        // ランダムにvoidを配置する（10%の確率）
        for (std::size_t z = 0; z < mapSizeZ_; ++z) {
            if (rand() % 10 == 0) { // 10%の確率
                mapChips_.back()[z] = MapChipType::Void;
            }
        }
        // ランダムに水場を配置する（10%の確率）
        for (std::size_t z = 0; z < mapSizeZ_; ++z) {
            if (rand() % 10 == 0) { // 10%の確率
                mapChips_.back()[z] = MapChipType::Water;
            }
        }
        // ランダムに資源を配置する（5%の確率で資源を配置）
        for (std::size_t z = 0; z < mapSizeZ_; ++z) {
            if (rand() % 20 == 0) { // 5%の確率
                mapChips_.back()[z] = MapChipType::Resource;
            }
        }
        // 駅を建設する間隔で駅チップを配置する
        if (mapSizeZ_ > 0 && (mapChips_.size() - 1) % stationBuildInterval_ == 0) {
            std::size_t stationZ = rand() % mapSizeZ_; // ランダムなZ座標に駅を配置
            mapChips_.back()[stationZ] = MapChipType::Station;
        }
    }
}

GameComponents::MapGeneratorComponent::MapData
GameComponents::MapGeneratorComponent::LoadCsv(const std::string& path, std::size_t width) const {
    // ファイルが無い場合も、ランダム生成の区画幅は失わない。
    MapData result(width, std::vector<MapChipType>(mapSizeZ_, MapChipType::Void));
    const std::filesystem::path csvPath(std::u8string(path.begin(), path.end()));
    std::ifstream input(csvPath, std::ios::binary);
    if (!input) {
        Logger::GetInstance().Warnf(LogCategory::Game,
            "MapGenerator: CSVを読み込めません。Voidで生成します: {}", path);
        return result;
    }

    std::size_t invalidCount = 0;
    std::string line;
    for (std::size_t z = 0; z < mapSizeZ_ && std::getline(input, line); ++z) {
        // UTF-8 BOM付きCSVにも対応する。空行は飛ばさずVoidの行として数える。
        if (z == 0 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);
        }
        const auto cells = SplitCsvRow(line);
        if (width == 0 && result.size() < cells.size()) {
            result.resize(cells.size(), std::vector<MapChipType>(mapSizeZ_, MapChipType::Void));
        }
        for (std::size_t x = 0; x < std::min(cells.size(), result.size()); ++x) {
            result[x][z] = ParseChip(cells[x], invalidCount);
        }
    }
    if (invalidCount > 0) {
        Logger::GetInstance().Warnf(LogCategory::Game,
            "MapGenerator: CSV内の不明なチップ {} 個をVoidにしました: {}", invalidCount, path);
    }
    return result;
}

void GameComponents::MapGeneratorComponent::AddCsvMapChips(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (settings_.mode == MapGenerationMode::FixedCsv && mapChips_.size() < fixedCsv_.size()) {
            mapChips_.push_back(fixedCsv_[mapChips_.size()]);
        } else if (settings_.mode == MapGenerationMode::RandomCsvPool) {
            // 描画側とレール側から小刻みに延長されても、区画の途中で再抽選しない。
            if (activeCsvColumn_ == 0) {
                activeCsvPoolIndex_ = selectedCsvPoolIndex_;
                if (activeCsvPoolIndex_ && !csvPools_[*activeCsvPoolIndex_].maps.empty()) {
                    const auto& maps = csvPools_[*activeCsvPoolIndex_].maps;
                    std::uniform_int_distribution<std::size_t> pick(0, maps.size() - 1);
                    activeCsvIndex_ = pick(csvRandom_);
                }
            }
            if (activeCsvPoolIndex_ && !csvPools_[*activeCsvPoolIndex_].maps.empty()) {
                mapChips_.push_back(csvPools_[*activeCsvPoolIndex_].maps[activeCsvIndex_][activeCsvColumn_]);
            } else {
                // 空プールでも区画幅を維持し、次の切替は区画境界で行う。
                mapChips_.emplace_back(mapSizeZ_, MapChipType::Void);
            }
            activeCsvColumn_ = (activeCsvColumn_ + 1) % settings_.csvChunkSizeX;
        } else {
            // 固定CSVの終端以降や空のプールは、従来のランダム生成へ戻さずVoidにする。
            mapChips_.emplace_back(mapSizeZ_, MapChipType::Void);
        }
    }
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

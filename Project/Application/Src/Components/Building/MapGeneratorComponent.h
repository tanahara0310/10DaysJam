#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>

#include "MapChipData.h"

namespace GameComponents
{
    // マップを生成するコンポーネント
    class MapGeneratorComponent final
        : public CoreEngine::IComponent {
    public:
        explicit MapGeneratorComponent(uint32_t mapSizeZ = 10, uint32_t startGenerateX = 30);

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "MapGenerator";
        }

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // X方向に指定された列数までマップを生成する
        void CreateToX(std::size_t xCount);
        // 追加でマップを生成する
        void AddMapChips(std::size_t count);

        // 指定マスの種類を取得・変更する
        MapChipType GetMapChip(std::size_t x, std::size_t z) const;
        bool SetMapChip(std::size_t x, std::size_t z, MapChipType type);

        // マップチップの2D配列を取得する
        const std::vector<std::vector<GameComponents::MapChipType>>& GetMapChips() const;

    private:
        uint32_t mapSizeZ_ = 10;
        float gridSize_ = 5.0f;

        uint32_t stationBuildInterval_ = 5; // 駅を建設する間隔（X方向のマス数）

        std::vector<std::vector<GameComponents::MapChipType>> mapChips_;
    };
}

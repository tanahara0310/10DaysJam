#pragma once

#include "GameObject/Component/Core/IComponent.h"

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

        // マップをXまで生成する
        void CreateToZ(uint32_t x);
        // 追加でマップを生成する
        void AddMapChips(uint32_t count);

        // マップチップの2D配列を取得する
        const std::vector<std::vector<GameComponents::MapChipType>>& GetMapChips();

    private:
        uint32_t mapSizeZ_ = 10;
        float gridSize_ = 5.0f;

        std::vector<std::vector<GameComponents::MapChipType>> mapChips_;
    };
}

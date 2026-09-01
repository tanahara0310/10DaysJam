#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace GameComponents
{
    // レールの配置、撤去を管理するコンポーネント
    class RailPathComponent final
        : public CoreEngine::IComponent {
    public:
        explicit RailPathComponent(
            uint32_t mapSizeZ = 10,
            uint32_t startX = 0, uint32_t startZ = 0);

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "RailPath";
        }

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // レールを設置する
        void PlaceRail(int32_t x, int32_t z);
        // レールを撤去する
        void RemoveRail(int32_t x, int32_t z);
        // 最後に設置したレールを撤去する（Undo）。消去後の最新座標を返し、残っていなければ {-1, -1} を返す
        std::pair<int32_t, int32_t> UndoLastRailPlacement();

        // キューにあるレールの先頭を確定する
        bool ConfirmNextRailPlacement();

        // 次に列車が進む未確定レールを取得する。存在しなければ false
        bool TryGetNextUnconfirmedRail(std::pair<int32_t, int32_t>& destination) const;
        // 未確定レールの数を取得する
        std::size_t GetUnconfirmedRailCount() const;

        // レールマップを取得する
        std::vector<std::pair<int32_t, int32_t>>& GetRailMap();
        // Z方向のマップサイズを取得する。X正方向には上限を設けない
        uint32_t GetMapSizeZ() const;

        // Undoスタックを取得する
        const std::vector<std::pair<int32_t, int32_t>>& GetRailUndoStack() const;

    private:
        uint32_t mapSizeZ_ = 10;

        // 確定したレールの座標を保持するマップ（2D配列）
        std::vector<std::pair<int32_t, int32_t>> railMap_;

        // Undo/Redo スタック
        std::vector<std::pair<int32_t, int32_t>> railUndoStack_;
    };
}

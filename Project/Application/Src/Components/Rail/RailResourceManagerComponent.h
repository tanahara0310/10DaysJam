#pragma once

#include "GameObject/Component/Core/IComponent.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace GameComponents
{
    // レールの配置、撤去を管理するコンポーネント
    class RailResourceManagerComponent final
        : public CoreEngine::IComponent {
    public:
        explicit RailResourceManagerComponent(
            uint32_t resourceCount)
            : initialResourceCount_(resourceCount), resourceCount_(resourceCount) {
        }

        // コンポーネントを識別する名前。必須
        const char* GetTypeName() const override {
            return "RailResourceManager";
        }

        json OnSerialize() const override;
        void OnDeserialize(const json& j) override;

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "レール所持数"; }
        bool DrawInspector() override;
#endif

        // 最初の更新直前に一度だけ呼ばれる
        void Start() override;
        // 毎フレーム呼ばれる
        void Update() override;

        // リソースの現在数を取得する
        void AddResource(uint32_t amount);
        // リソースを消費する
        bool UseResource(uint32_t amount);
        // リソースが指定量以上あるかを確認する
        bool HasEnoughResource(uint32_t amount) const;
        // 現在のレール数を取得する
        uint32_t GetResourceCount() const { return resourceCount_; }

    private:
        uint32_t initialResourceCount_ = 15;
        uint32_t resourceCount_ = 15; // 初期リソース数
    };
}

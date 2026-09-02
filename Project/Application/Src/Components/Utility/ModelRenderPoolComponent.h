#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Math/Vector/Vector3.h"
#include "Math/Vector/Vector4.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace CoreEngine
{
    class GameObject;
    class TransformComponent;
}

namespace GameComponents
{
    /// @brief Draw() が呼ばれた分だけ、同じ静的モデルの GameObject をプールから表示する。
    /// @note 表示したいモデルは毎フレーム Draw() する。呼ばれなかった要素は自動で非表示になる。
    class ModelRenderPoolComponent final : public CoreEngine::IComponent {
    public:
        explicit ModelRenderPoolComponent(
            std::string modelPath,
            std::size_t initialCapacity = 32,
            bool allowGrowth = true,
            std::optional<CoreEngine::Vector4> color = std::nullopt)
            : modelPath_(std::move(modelPath)),
              initialCapacity_(initialCapacity),
              allowGrowth_(allowGrowth),
              color_(color) {
        }

        const char* GetTypeName() const override {
            return "ModelRenderPool";
        }

        /// @brief initialCapacity 分のオブジェクトを生成する。
        void Awake() override;
        /// @brief このフレームに Draw() されなかった要素を非表示にする。
        void Update() override;
        /// @brief コンポーネントだけが外された場合も、生成したオブジェクトを残さない。
        void OnDestroy() override;

        /// @brief このフレームにモデルを1つ表示する。
        /// @return 描画用オブジェクトを割り当てられた場合 true。
        bool Draw(
            const CoreEngine::Vector3& position,
            const CoreEngine::Vector3& rotation = { 0.0f, 0.0f, 0.0f },
            const CoreEngine::Vector3& scale = { 1.0f, 1.0f, 1.0f });

        std::size_t GetCapacity() const { return entries_.size(); }
        std::size_t GetActiveCount() const;
        std::size_t GetAvailableCount() const {
            return GetCapacity() - GetActiveCount();
        }

    private:
        struct Entry {
            CoreEngine::GameObject* object = nullptr;
            CoreEngine::TransformComponent* transform = nullptr;
            std::uint64_t lastSubmittedFrame =
                (std::numeric_limits<std::uint64_t>::max)();
        };

        Entry* CreateEntry();
        Entry* FindAvailableEntry(std::uint64_t frame);

        std::string modelPath_;
        std::size_t initialCapacity_ = 32;
        bool allowGrowth_ = true;
        std::optional<CoreEngine::Vector4> color_;
        std::uint64_t allocationFrame_ =
            (std::numeric_limits<std::uint64_t>::max)();
        std::size_t nextEntryIndex_ = 0;
        std::uint64_t lastExhaustedWarningFrame_ =
            (std::numeric_limits<std::uint64_t>::max)();
        std::vector<Entry> entries_;
    };
}

#pragma once

#include "ShootingEvents.h"

#include "Collision/ColliderComponent.h"
#include "Collision/CollisionLayer.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Core/IComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Graphics/Primitive/CubeMeshGenerator.h"
#include "Math/Easing/EasingUtil.h"
#include "Utility/Event/EventBus.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Random/RandomGenerator.h"
#include "Utility/Tween/Tween.h"

#include <memory>

namespace ShootingSample
{
    /// @brief 敵を手前へ直進させ、登場・死亡の演出を持つコンポーネント。
    class EnemyComponent : public CoreEngine::IComponent {
    public:
        static constexpr int kScore = 100;

        const char* GetTypeName() const override { return "Enemy"; }

        /// @brief 登場演出を開始する
        void Start() override
        {
            transform_ = Sibling<CoreEngine::TransformComponent>();
            if (!transform_) { return; }

            // 潰れた状態から膨らませる。EaseOutBack で一度行き過ぎてから収まるので、
            // 同じ 0.3 秒でも「ポンと出た」感じが出る
            transform_->Get().scale = { 0.05f, 0.05f, 0.05f };
            CoreEngine::Tween::ScaleTo(GetOwner(), { 1.0f, 1.0f, 1.0f }, 0.3f)
                .SetEase(CoreEngine::EasingUtil::Type::EaseOutBack);
        }

        /// @brief 手前へ進め、通り過ぎたら撃ち漏らしとして消す
        void Update() override
        {
            if (!transform_ || dying_) { return; }

            auto& world = transform_->Get();
            world.translate.z -= kSpeed * CoreEngine::Time::DeltaTime();

            if (world.translate.z < kDespawnZ) {
                // 「撃ち漏らした」という事実だけを投げる。
                // コンボを切るのは HUD 側の判断であって、敵の関知するところではない
                CoreEngine::EventBus::GetInstance().Queue(EnemyEscapedEvent{});
                GetOwner()->Destroy();
            }
        }

        /// @brief 撃破されたときに呼ばれる
        /// @param awardScore 得点を与えるか（自機とぶつかった場合は与えない）
        /// @note すぐには消さず、縮んで消える演出を挟んでから Destroy する。
        void Die(bool awardScore)
        {
            if (dying_) { return; } // 同フレームに 2 発当たっても 1 回しか死なない
            dying_ = true;

            // 死んでいる間に当たり判定が残っていると、後続の弾を吸い込んでしまう。
            // コンポーネントごと止めれば CollisionWorld への登録から外れる
            if (auto* colliders = GetOwner()->TryGetColliders()) {
                colliders->SetEnabled(false);
            }

            if (awardScore && transform_) {
                CoreEngine::EventBus::GetInstance().Queue(
                    EnemyDiedEvent{ transform_->Get().translate, kScore });
            }

            // 登場演出がまだ動いていることがあるので、先に止めてから死亡演出を載せる。
            // 同じ scale を 2 本のトゥイーンが取り合うのを防ぐ
            CoreEngine::Tween::KillByLink(GetOwner());

            CoreEngine::Tween::ScaleTo(GetOwner(), { 0.0f, 0.0f, 0.0f }, 0.22f)
                .SetEase(CoreEngine::EasingUtil::Type::EaseInBack)
                .OnComplete([owner = GetOwner()] { owner->Destroy(); });

            CoreEngine::Tween::RotateTo(GetOwner(), { 0.0f, kDeathSpin, 0.0f }, 0.22f);
        }

    private:
        static constexpr float kSpeed = 6.0f;
        static constexpr float kDespawnZ = -16.0f;
        static constexpr float kDeathSpin = 4.0f; // ラジアン

        CoreEngine::TransformComponent* transform_ = nullptr;
        bool dying_ = false;
    };

    /// @brief 敵 1 体分の構成。
    class EnemyObject : public CoreEngine::GameObject {
    public:
        static constexpr float kSize = 1.0f;

        /// @brief Hierarchy の表示名（Enemy_0, Enemy_1 ... と自動採番される）
        const char* GetObjectName() const override { return "Enemy"; }

        void Initialize() override
        {
            AddComponent<CoreEngine::MeshRendererComponent>(
                std::make_unique<CoreEngine::CubeMeshGenerator>(kSize));
            AddComponent<CoreEngine::MaterialComponent>()
                ->SetColor({ 0.90f, 0.30f, 0.28f, 1.0f });

            AddAABBCollider({ kSize, kSize, kSize }, CoreEngine::CollisionLayer::Enemy);
            AddComponent<EnemyComponent>();
        }
    };

    /// @brief 一定間隔で敵を湧かせるコンポーネント。
    /// @details 見た目を持たない空の GameObject に載せて使う。
    class EnemySpawnerComponent : public CoreEngine::IComponent {
    public:
        const char* GetTypeName() const override { return "EnemySpawner"; }

        void Update() override
        {
            timer_ -= CoreEngine::Time::DeltaTime();
            if (timer_ > 0.0f) { return; }
            timer_ = kInterval;

            // Update 中の Spawn は安全（次フレームから動き始める）
            auto* enemy = GetOwner()->Spawn<EnemyObject>();

            const float x = CoreEngine::RandomGenerator::GetInstance().GetFloat(-kSpreadX, kSpreadX);
            enemy->GetComponent<CoreEngine::TransformComponent>()->Get().translate =
                { x, EnemyObject::kSize * 0.5f, kSpawnZ };
        }

    private:
        static constexpr float kInterval = 0.8f;
        static constexpr float kSpawnZ = 20.0f;
        static constexpr float kSpreadX = 9.0f;

        float timer_ = 0.5f;
    };
}

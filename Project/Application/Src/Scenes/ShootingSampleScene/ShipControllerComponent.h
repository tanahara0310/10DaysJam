#pragma once

#include "BulletObject.h"
#include "EnemyObject.h"
#include "ShootingEvents.h"

#include "Collision/ColliderComponent.h"
#include "Collision/CollisionInfo.h"
#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Core/IComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Input/InputManager.h"
#include "Math/Easing/EasingUtil.h"
#include "Math/Vector/Vector4.h"
#include "Utility/Event/EventBus.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Tween/Tween.h"

#include <algorithm>

namespace ShootingSample
{
    /// @brief 自機の移動・弾の発射・被弾処理を行うコンポーネント。
    class ShipControllerComponent : public CoreEngine::IComponent {
    public:
        static constexpr int kMaxLife = 3;

        const char* GetTypeName() const override { return "ShipController"; }

        /// @brief 兄弟コンポーネントの取得と被弾コールバックの購読を行う
        void Start() override
        {
            transform_ = Sibling<CoreEngine::TransformComponent>();
            material_ = Sibling<CoreEngine::MaterialComponent>();

            ApplyBaseColor();

            GetOwner()->GetColliders().SetOnEnter(
                [this](const CoreEngine::CollisionInfo& info) { OnHit(info); });
        }

        /// @brief 移動と発射
        void Update() override
        {
            if (!transform_) { return; }

            const float deltaTime = CoreEngine::Time::DeltaTime();
            fireCooldown_ -= deltaTime;

            const CoreEngine::InputQuery* input = Input();
            if (!input) { return; }

            using CoreEngine::InputAction;

            const float moveX = input->GetAxisValue(InputAction::MoveRight)
                - input->GetAxisValue(InputAction::MoveLeft);
            const float moveZ = input->GetAxisValue(InputAction::MoveForward)
                - input->GetAxisValue(InputAction::MoveBack);

            auto& world = transform_->Get();
            world.translate.x = std::clamp(
                world.translate.x + moveX * kMoveSpeed * deltaTime, -kLimitX, kLimitX);
            world.translate.z = std::clamp(
                world.translate.z + moveZ * kMoveSpeed * deltaTime, kMinZ, kMaxZ);

            // 進行方向へ軽く傾ける（トゥイーンを使わず即時に追従させたいので直接書く）
            world.rotate.z = -moveX * kBankRadians;

            // Attack = マウス左 / パッドX。押しっぱなしで連射する
            if (input->IsActionPressed(InputAction::Attack) && fireCooldown_ <= 0.0f) {
                fireCooldown_ = kFireInterval;
                Fire();
            }
        }

        /// @brief 残り機数
        int GetLife() const { return life_; }

    private:
        static constexpr float kMoveSpeed = 12.0f;
        static constexpr float kLimitX = 10.0f;
        static constexpr float kMinZ = -13.0f;
        static constexpr float kMaxZ = 2.0f;
        static constexpr float kFireInterval = 0.16f;
        static constexpr float kBulletSpeed = 26.0f;
        static constexpr float kBankRadians = 0.35f;
        static constexpr float kInvincibleSeconds = 1.2f;

        /// @brief 自機の前方へ弾を 1 発スポーンする
        void Fire()
        {
            auto* bullet = GetOwner()->Spawn<BulletObject>();

            bullet->GetComponent<CoreEngine::TransformComponent>()->Get().translate =
                transform_->Get().translate + CoreEngine::Vector3{ 0.0f, 0.0f, 1.0f };
            bullet->GetComponent<BulletComponent>()->SetVelocity(
                { 0.0f, 0.0f, kBulletSpeed });

            // 発射の反動。0.05 秒で潰れて 0.12 秒で戻る
            CoreEngine::Tween::KillById("shipRecoil");
            CoreEngine::TweenSequence()
                .Append(CoreEngine::Tween::ScaleTo(GetOwner(), { 1.0f, 1.0f, 0.82f }, 0.05f))
                .Append(CoreEngine::Tween::ScaleTo(GetOwner(), { 1.0f, 1.0f, 1.0f }, 0.12f)
                    .SetEase(CoreEngine::EasingUtil::Type::EaseOutBack))
                .SetLink(GetOwner())
                .SetId("shipRecoil");
        }

        /// @brief 敵と接触したときの処理
        void OnHit(const CoreEngine::CollisionInfo& info)
        {
            // ぶつかった敵は得点なしで消す（体当たりで稼げてしまうのを防ぐ）
            if (info.other) {
                if (auto* enemy = info.other->GetComponent<EnemyComponent>()) {
                    enemy->Die(false);
                } else {
                    info.other->Destroy();
                }
            }

            if (invincible_) { return; }

            life_ = (life_ > 0) ? (life_ - 1) : 0;

            // 「自機がダメージを受けた」という事実だけを投げる。
            // HUD の更新・コンボ切断・画面演出は、それぞれが勝手に反応する
            CoreEngine::EventBus::GetInstance().Queue(PlayerDamagedEvent{ life_ });

            PlayDamageEffect();
        }

        /// @brief 被弾表現（赤フラッシュ ＋ 無敵中の点滅）
        /// @note 以前は damageFlash_ という変数と Update 内の分岐で書いていた処理。
        ///       トゥイーンにすると「補間なしの色切り替え」だったものが素直に減衰する。
        void PlayDamageEffect()
        {
            if (!material_) { return; }

            invincible_ = true;

            CoreEngine::Tween::KillById("shipDamage");

            auto setColor = [this](const CoreEngine::Vector4& color) {
                if (material_) { material_->SetColor(color); }
                };

            CoreEngine::TweenSequence()
                // 一瞬で赤くして、ゆっくり戻す
                .Append(CoreEngine::Tween::To<CoreEngine::Vector4>(
                    kBaseColor, kHitColor, 0.04f, setColor))
                .Append(CoreEngine::Tween::To<CoreEngine::Vector4>(
                    kHitColor, kBaseColor, 0.30f, setColor)
                    .SetEase(CoreEngine::EasingUtil::Type::EaseOutCubic))
                // 無敵時間のあいだ点滅させる
                .Append(CoreEngine::Tween::To<CoreEngine::Vector4>(
                    kBaseColor, kFadedColor, kInvincibleSeconds / 8.0f, setColor)
                    .SetLoops(8, CoreEngine::TweenLoop::Yoyo))
                .AppendCallback([this] {
                    invincible_ = false;
                    ApplyBaseColor();
                    })
                .SetLink(GetOwner())
                .SetId("shipDamage");
        }

        void ApplyBaseColor()
        {
            if (material_) { material_->SetColor(kBaseColor); }
        }

        const CoreEngine::InputQuery* Input() const
        {
            auto* engine = GetOwner() ? GetOwner()->GetEngineSystem() : nullptr;
            auto* manager = engine ? engine->GetService<CoreEngine::InputManager>() : nullptr;
            return manager ? &manager->GetQuery() : nullptr;
        }

        static inline const CoreEngine::Vector4 kBaseColor{ 0.30f, 0.65f, 0.95f, 1.0f };
        static inline const CoreEngine::Vector4 kHitColor{ 0.95f, 0.35f, 0.25f, 1.0f };
        static inline const CoreEngine::Vector4 kFadedColor{ 0.30f, 0.65f, 0.95f, 0.25f };

        CoreEngine::TransformComponent* transform_ = nullptr;
        CoreEngine::MaterialComponent* material_ = nullptr;
        float fireCooldown_ = 0.0f;
        int life_ = kMaxLife;
        bool invincible_ = false;
    };
}

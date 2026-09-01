#include "pch.h"
#include "ShootingSampleScene.h"

#include "BulletObject.h"
#include "EnemyObject.h"
#include "ScoreHudComponent.h"
#include "ShipControllerComponent.h"

#include "Collision/CollisionLayer.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Graphics/Primitive/CubeMeshGenerator.h"
#include "Graphics/Primitive/PlaneMeshGenerator.h"
#include "UI/UIText.h"

#include <memory>

namespace ShootingSample
{
    using namespace CoreEngine;

    namespace {
        constexpr float kFieldWidth = 24.0f;
        constexpr float kFieldDepth = 40.0f;
        constexpr float kFieldCenterZ = 4.0f;

        constexpr float kShipWidth = 1.0f;
        constexpr float kShipHeight = 0.4f;
        constexpr float kShipDepth = 1.4f;

        /// HUD の描画順。数字が大きいほど前面
        constexpr int kHudSortOrder = 100;
    }

    void ShootingSampleScene::OnInitialize()
    {
        SetSceneName("ShootingSampleScene");

        // 自機の背後上空から奥を見下ろす
        SetReleaseCameraTransform({ 0.0f, 15.0f, -24.0f }, { 0.52f, 0.0f, 0.0f });
        SetReleaseCameraLens(50.0f, 500.0f);

        // 判定するレイヤーの組み合わせは既定で無効なので、ここで開通させる
        SetCollisionEnabled(CollisionLayer::PlayerBullet, CollisionLayer::Enemy, true);
        SetCollisionEnabled(CollisionLayer::Player, CollisionLayer::Enemy, true);

        // ── フィールド ────────────────────────────────────
        {
            auto* field = CreateObject("Field");
            field->AddComponent<MeshRendererComponent>(
                std::make_unique<PlaneMeshGenerator>(kFieldWidth, kFieldDepth));
            field->AddComponent<MaterialComponent>()->SetColor({ 0.14f, 0.17f, 0.22f, 1.0f });

            // 既定の床と Z ファイトしないよう少しだけ持ち上げる
            field->GetComponent<TransformComponent>()->Get().translate =
                { 0.0f, 0.02f, kFieldCenterZ };
        }

        // ── 自機 ──────────────────────────────────────────
        ShipControllerComponent* ship = nullptr;
        {
            auto* shipObject = CreateObject("Ship");
            shipObject->AddComponent<MeshRendererComponent>(
                std::make_unique<CubeMeshGenerator>(kShipWidth, kShipHeight, kShipDepth));
            shipObject->AddComponent<MaterialComponent>();
            shipObject->GetComponent<TransformComponent>()->Get().translate =
                { 0.0f, kShipHeight * 0.5f, -10.0f };

            shipObject->AddAABBCollider({ kShipWidth, kShipHeight, kShipDepth }, CollisionLayer::Player);
            ship = shipObject->AddComponent<ShipControllerComponent>();
        }

        // ── 敵スポナー（見た目を持たない管理用オブジェクト）──
        {
            auto* spawner = CreateObject("EnemySpawner");
            spawner->SetSerializeEnabled(false);
            spawner->AddComponent<EnemySpawnerComponent>();
        }

        // ── HUD ───────────────────────────────────────────
        // テキストはフォント指定なしで作れる（FontManager の既定フォントが自動で使われ、
        // 収録されていない文字は実行時にアトラスへ焼かれる）
        BuildHud(ship);
    }

    void ShootingSampleScene::BuildHud(ShipControllerComponent* ship)
    {
        auto* scoreText = CreateText("SCORE 0",
            ScoreHudComponent::kScoreFontSize, UIAnchor::TopLeft,
            ScoreHudComponent::kScorePos, ScoreHudComponent::kScoreColor, "ScoreText");

        auto* comboText = CreateText("COMBO",
            ScoreHudComponent::kComboFontSize, UIAnchor::TopLeft,
            ScoreHudComponent::kComboPos, ScoreHudComponent::kComboIdleColor, "ComboText");

        auto* lifeText = CreateText("LIFE",
            ScoreHudComponent::kLifeFontSize, UIAnchor::TopLeft,
            ScoreHudComponent::kLifePos, ScoreHudComponent::kLifeColor, "LifeText");

        auto* hintText = CreateText("WASD / 左スティックで移動　マウス左 / パッドXで発射",
            20.0f, UIAnchor::BottomLeft, { 32.0f, -44.0f },
            { 0.55f, 0.58f, 0.65f, 1.0f }, "HintText");

        // 毎回コードから作り直すので、シーン JSON には書き出さない
        // （書き出すと保存・再読み込みのたびに二重に生えてしまう）
        for (UIText* text : { scoreText, comboText, lifeText, hintText }) {
            if (!text) { continue; }
            text->SetSerializeEnabled(false);
            text->SetSortOrder(kHudSortOrder);
        }

        auto* hud = CreateObject("Hud");
        hud->SetSerializeEnabled(false);

        auto* hudComponent = hud->AddComponent<ScoreHudComponent>();
        hudComponent->SetTexts(scoreText, comboText, lifeText);
        hudComponent->SetLife(ship ? ship->GetLife() : ShipControllerComponent::kMaxLife);
    }
}

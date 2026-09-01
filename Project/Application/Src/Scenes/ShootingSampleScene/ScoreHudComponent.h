#pragma once

#include "ShootingEvents.h"

#include "GameObject/GameObject.h"
#include "GameObject/Component/Core/IComponent.h"
#include "Math/Easing/EasingUtil.h"
#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector4.h"
#include "UI/UIText.h"
#include "Utility/Event/EventBus.h"
#include "Utility/Tween/Tween.h"

#include <string>

namespace ShootingSample
{
    /// @brief スコア・コンボ・残機を表示する HUD。
    ///
    /// @details
    ///  このコンポーネントは **弾も敵も自機も知りません**。
    ///  知っているのは「EnemyDiedEvent が来たら加点する」ということだけです。
    ///  そのため HUD を丸ごと消しても、弾・敵・自機のコードは 1 行も変わりません。
    ///  逆に SE やエフェクトを足すときも、同じイベントを購読するコンポーネントを
    ///  1 つ増やすだけで済みます。
    class ScoreHudComponent : public CoreEngine::IComponent {
    public:
        const char* GetTypeName() const override { return "ScoreHud"; }

        /// @brief 表示に使うテキストを渡す（生成はシーン側が行う）
        void SetTexts(CoreEngine::UIText* score, CoreEngine::UIText* combo, CoreEngine::UIText* life)
        {
            scoreText_ = score;
            comboText_ = combo;
            lifeText_ = life;
        }

        /// @brief 初期残機を設定する
        void SetLife(int life) { life_ = life; }

        /// @brief イベントを購読して初期表示を作る
        void Start() override
        {
            auto& bus = CoreEngine::EventBus::GetInstance();

            // 敵を倒した → 加点してコンボを伸ばす
            bag_.Add(bus.Subscribe<EnemyDiedEvent>(
                [this](const EnemyDiedEvent& e) { OnEnemyDied(e); }));

            // 撃ち漏らした → コンボが切れる
            bag_.Add(bus.Subscribe<EnemyEscapedEvent>(
                [this](const EnemyEscapedEvent&) { BreakCombo(); }));

            // 被弾した → コンボが切れて残機表示が減る
            bag_.Add(bus.Subscribe<PlayerDamagedEvent>(
                [this](const PlayerDamagedEvent& e) { OnPlayerDamaged(e); }));

            RefreshScoreText();
            RefreshComboText();
            RefreshLifeText();
        }

    private:
        // ──────────────────────────────────────────────────────────
        // イベントへの反応
        // ──────────────────────────────────────────────────────────

        void OnEnemyDied(const EnemyDiedEvent& e)
        {
            ++combo_;
            score_ += e.score * combo_;

            // スコアは一気に飛ばさず、転がるように増やす。
            // displayedScore_ をトゥイーンして、その値を毎フレーム文字へ写す
            CoreEngine::Tween::KillById("scoreCount");
            CoreEngine::Tween::To(&displayedScore_, static_cast<float>(score_), 0.5f)
                .SetEase(CoreEngine::EasingUtil::Type::EaseOutQuart)
                .SetLink(GetOwner())
                .SetId("scoreCount")
                .OnUpdate([this](float) { RefreshScoreText(); })
                .OnComplete([this] { RefreshScoreText(); });

            PunchText(scoreText_, kScoreFontSize, "scorePunch");

            RefreshComboText();
            if (combo_ >= 2) {
                PunchText(comboText_, kComboFontSize, "comboPunch");
            }
        }

        void OnPlayerDamaged(const PlayerDamagedEvent& e)
        {
            life_ = e.remainingLife;
            BreakCombo();
            RefreshLifeText();

            if (!lifeText_) { return; }

            // 赤く光らせてから戻す
            const CoreEngine::Vector4 danger{ 1.0f, 0.35f, 0.30f, 1.0f };
            auto setColor = [this](const CoreEngine::Vector4& color) {
                if (lifeText_) { lifeText_->SetColor(color); }
                };

            CoreEngine::Tween::KillById("lifeFlash");
            CoreEngine::TweenSequence()
                .Append(CoreEngine::Tween::To<CoreEngine::Vector4>(kLifeColor, danger, 0.05f, setColor))
                .Append(CoreEngine::Tween::To<CoreEngine::Vector4>(danger, kLifeColor, 0.45f, setColor)
                    .SetEase(CoreEngine::EasingUtil::Type::EaseOutCubic))
                .SetLink(lifeText_)
                .SetId("lifeFlash");

            // 左右に細かく揺らす
            const CoreEngine::Vector2 basePos = kLifePos;
            auto setPos = [this](const CoreEngine::Vector2& pos) {
                if (lifeText_) { lifeText_->SetAnchoredPosition(pos); }
                };

            CoreEngine::Tween::KillById("lifeShake");
            CoreEngine::Tween::To<CoreEngine::Vector2>(
                { basePos.x - kShakeAmount, basePos.y },
                { basePos.x + kShakeAmount, basePos.y }, 0.05f, setPos)
                .SetLoops(6, CoreEngine::TweenLoop::Yoyo)
                .SetLink(lifeText_)
                .SetId("lifeShake")
                .OnComplete([this, basePos] {
                    if (lifeText_) { lifeText_->SetAnchoredPosition(basePos); }
                    });
        }

        void BreakCombo()
        {
            if (combo_ == 0) { return; }
            combo_ = 0;
            RefreshComboText();
        }

        // ──────────────────────────────────────────────────────────
        // 表示
        // ──────────────────────────────────────────────────────────

        void RefreshScoreText()
        {
            if (!scoreText_) { return; }
            scoreText_->SetText("SCORE " + std::to_string(static_cast<int>(displayedScore_ + 0.5f)));
        }

        void RefreshComboText()
        {
            if (!comboText_) { return; }

            if (combo_ >= 2) {
                comboText_->SetText(std::to_string(combo_) + " COMBO");
                comboText_->SetColor(kComboActiveColor);
            } else {
                // 空文字にするとフィールドが潰れて次の表示位置がぶれるので、
                // 見出しだけ残して暗くしておく
                comboText_->SetText("COMBO");
                comboText_->SetColor(kComboIdleColor);
            }
        }

        void RefreshLifeText()
        {
            if (!lifeText_) { return; }

            if (life_ <= 0) {
                lifeText_->SetText("GAME OVER");
                return;
            }

            std::string body = "LIFE ";
            for (int i = 0; i < life_; ++i) { body += "■"; }
            lifeText_->SetText(body);
        }

        /// @brief 文字を一瞬だけ大きくして戻す（加点の手応え）
        /// @param text 対象
        /// @param baseSize 通常時のフォントサイズ
        /// @param id 重ねがけを防ぐための識別子
        static void PunchText(CoreEngine::UIText* text, float baseSize, const char* id)
        {
            if (!text) { return; }

            // 連続で倒したときに前の演出が残っていると、サイズが戻らないまま積み重なる
            CoreEngine::Tween::KillById(id);

            CoreEngine::Tween::To<float>(baseSize * kPunchScale, baseSize, 0.32f,
                [text](float size) { text->SetFontSize(size); })
                .SetEase(CoreEngine::EasingUtil::Type::EaseOutBack)
                .SetLink(text)
                .SetId(id);
        }

    public:
        // シーン側がテキストを作るときに同じ値を使うので公開する
        static constexpr float kScoreFontSize = 40.0f;
        static constexpr float kComboFontSize = 28.0f;
        static constexpr float kLifeFontSize = 28.0f;

        static inline const CoreEngine::Vector2 kScorePos{ 32.0f, 26.0f };
        static inline const CoreEngine::Vector2 kComboPos{ 32.0f, 76.0f };
        static inline const CoreEngine::Vector2 kLifePos{ 32.0f, 116.0f };

        static inline const CoreEngine::Vector4 kScoreColor{ 1.0f, 0.95f, 0.75f, 1.0f };
        static inline const CoreEngine::Vector4 kComboIdleColor{ 0.55f, 0.58f, 0.65f, 1.0f };
        static inline const CoreEngine::Vector4 kComboActiveColor{ 1.0f, 0.75f, 0.30f, 1.0f };
        static inline const CoreEngine::Vector4 kLifeColor{ 0.65f, 0.85f, 1.0f, 1.0f };

    private:
        static constexpr float kPunchScale = 1.35f;
        static constexpr float kShakeAmount = 6.0f;

        CoreEngine::UIText* scoreText_ = nullptr;
        CoreEngine::UIText* comboText_ = nullptr;
        CoreEngine::UIText* lifeText_ = nullptr;

        int score_ = 0;
        float displayedScore_ = 0.0f; ///< 表示上のスコア（トゥイーンで追いかける）
        int combo_ = 0;
        int life_ = 3;

        /// @brief 購読の束。破棄時に全部まとめて解除される
        CoreEngine::SubscriptionBag bag_;
    };
}

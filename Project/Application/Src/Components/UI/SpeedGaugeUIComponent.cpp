#include "pch.h"
#include "SpeedGaugeUIComponent.h"

#include "Components/Train/TrainMovementComponent.h"
#include "GameObject/GameObject.h"
#include "Math/MathCore.h"
#include "EngineSystem/EngineSystem.h"
#include "Text/FontManager.h"
#include "UI/UIImage.h"
#include "UI/UIText.h"
#include "Utility/CVar/CVar.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#include "Editor/ImGui/CVarPanel.h"
#endif

using namespace CoreEngine;

namespace
{
    // ───────────────────────────────────────────────────────────────
    // テクスチャ。スタミナゲージと同じ版下を流用するので新規アセットは無い
    // ───────────────────────────────────────────────────────────────
    constexpr const char* kTexBoardMid = "Application/Assets/Textures/Stamina/board_mid.png";
    constexpr const char* kTexBoardCapL = "Application/Assets/Textures/Stamina/board_cap_l.png";
    constexpr const char* kTexBoardCapR = "Application/Assets/Textures/Stamina/board_cap_r.png";
    constexpr const char* kTexVine = "Application/Assets/Textures/Stamina/vine.png";

    // ───────────────────────────────────────────────────────────────
    // 版下の寸法。スタミナゲージの kArtScale と揃えること
    // ───────────────────────────────────────────────────────────────
    constexpr float kArtScale = 2.0f;

    constexpr float kPanelHeight = 38.0f * kArtScale;  ///< board_mid.png の高さと一致させる
    constexpr float kCapWidth = 14.0f * kArtScale;
    constexpr float kInnerPadding = 10.0f * kArtScale;
    /// 登場アニメーションで引っ込めるときに、板の右端を画面外へ出しておく余白 [px]
    constexpr float kIntroMargin = 48.0f;

    /// board_mid.png に焼かれている枠の太さ（上下それぞれ）。実測 12px なので中身は 52px しかない
    constexpr float kBoardFrameInset = 6.0f * kArtScale;
    /// 枠と数字のあいだに空ける余白。詰めると字が枠に触れて読みにくくなる
    constexpr float kDigitBreathing = 5.0f * kArtScale;
    constexpr float kVineWidth = 52.0f * kArtScale;
    constexpr float kVineHeight = 19.0f * kArtScale;
    constexpr std::size_t kVineCount = 6;
    constexpr float kVineOverhang = 6.0f * kArtScale;

    /// 数字 1 桁ぶんの送り幅。窓は描かないので、字が触れ合わない間隔だけ取る
    constexpr float kDigitWidth = 18.0f * kArtScale;
    constexpr float kDigitGap = 2.5f * kArtScale;
    constexpr float kDotWidth = 7.0f * kArtScale;
    constexpr float kDotSideGap = 2.0f * kArtScale;
    constexpr float kUnitGap = 6.0f * kArtScale;
    constexpr float kUnitWidth = 32.0f * kArtScale;   ///< "km/h" の見込み幅
    constexpr float kDigitFontSize = 19.0f * kArtScale;
    constexpr float kUnitFontSize = 11.0f * kArtScale;
    /// 小数点は数字より下に置くと、桁の並びが読みやすくなる
    constexpr float kDotBaselineOffset = 5.0f * kArtScale;

    /// ドット絵フォントの数字は em のおよそ 0.75 倍の高さになる
    constexpr float kDigitHeightRatio = 0.75f;
    // 数字が枠に触れていないことをここで担保する。フォントを大きくしたらここで気づける
    static_assert(
        kDigitFontSize * kDigitHeightRatio <=
        kPanelHeight - (kBoardFrameInset + kDigitBreathing) * 2.0f,
        "数字が板の枠に触れます。kDigitFontSize を下げるか kDigitBreathing を詰めてください");

    /// 整数 3 桁 ＋ 小数点 ＋ 小数 1 桁 ＋ 単位
    constexpr float kContentWidth =
        kDigitWidth * 3.0f + kDigitGap * 2.0f +
        kDotSideGap + kDotWidth + kDotSideGap +
        kDigitWidth +
        kUnitGap + kUnitWidth;

    // ───────────────────────────────────────────────────────────────
    // 桁送り
    // ───────────────────────────────────────────────────────────────
    constexpr float kRollSeconds = 0.14f;               ///< 1 桁が入れ替わる時間
    constexpr float kRollTravel = 4.5f * kArtScale;     ///< 送り出される距離

    /// 表示できる上限。3 桁＋小数 1 桁なので 999.9 km/h
    constexpr float kMaxDisplayKmh = 999.9f;

    // ───────────────────────────────────────────────────────────────
    // 針の振れ。停車中は 0 を出し、発車したら一気に上げる
    // ───────────────────────────────────────────────────────────────
    /// 上がるときの速さ [km/h 毎秒]。発車時に 0 から本来の速度まで 1 秒かからずに届く。
    /// 走行中の加速（既定で毎秒 0.72 km/h）はこれよりずっと緩いので、追いついた後は素通しになる
    constexpr float kRiseRate = 22.0f;
    /// 下がるときの速さ [km/h 毎秒]。駅の減速と停車がゆっくり落ちて見える
    constexpr float kFallRate = 38.0f;
    /// これ以下しか動いていなければ停車とみなす [ワールド単位]
    constexpr float kMovingEpsilonSquared = 1.0e-8f;

    // ───────────────────────────────────────────────────────────────
    // 色。スタミナゲージの粒（バナナ）と輪郭に合わせてある
    // ───────────────────────────────────────────────────────────────
    const Vector4 kDigitColor{ 0.980f, 0.839f, 0.200f, 1.0f };  ///< #FAD633
    const Vector4 kDotColor{ 0.573f, 0.341f, 0.024f, 1.0f };    ///< #925706
    const Vector4 kUnitColor{ 0.620f, 0.565f, 0.471f, 1.0f };   ///< #9E9078
    const Vector4 kOutlineColor{ 0.031f, 0.020f, 0.012f, 1.0f };///< #080503
    constexpr float kOutlineWidth = 0.045f;

    // ───────────────────────────────────────────────────────────────
    // 調整用 CVar（CVars.json へ自動保存され、インスペクターから編集できる）
    // ───────────────────────────────────────────────────────────────
    CVar<bool> cvEnabled{
        "Game.SpeedGauge.Enabled", true,
        "トロッコの速度計を表示する" };

    CVar<Vector2> cvPosition{
        "Game.SpeedGauge.Position", { 32.0f, 152.0f },
        "画面左上を基準にした位置 [px]（基準解像度 1920x1080）。"
        "既定はスタミナゲージ（y=44・高さ 76）の真下で、蔦どうしが触れない分だけ空けてある。"
        "Game.StaminaGauge.Position を動かしたらこちらも合わせること",
        CVarRange{ -2000.0f, 2000.0f } };

    CVar<float> cvScale{
        "Game.SpeedGauge.Scale", 1.0f,
        "速度計全体の表示倍率。1.0 はテクスチャの原寸なのでドットが一切ボケない",
        CVarRange{ 0.25f, 2.5f } };

    CVar<float> cvBoardBrightness{
        "Game.SpeedGauge.BoardBrightness", 0.68f,
        "板と端木の濃さ。1 でテクスチャそのまま、下げるほど濃い木になる",
        CVarRange{ 0.2f, 1.5f } };

    CVar<float> cvBoardGreenTint{
        "Game.SpeedGauge.BoardGreenTint", 0.22f,
        "板を緑へ寄せる強さ。上げるほど湿ったジャングルの木らしい色みになる",
        CVarRange{ 0.0f, 1.0f } };

    CVar<float> cvMetersPerCell{
        "Game.SpeedGauge.MetersPerCell", 2.0f,
        "レール 1 マスを何メートルとみなすか。km/h = 速度[マス/秒] x この値 x 3.6",
        CVarRange{ 0.1f, 20.0f } };

    CVar<int> cvSortOrder{
        "Game.SpeedGauge.SortOrder", 880,
        "速度計の描画順（大きいほど手前）",
        CVarRange{ 0.0f, 5000.0f } };

    CVar<float> cvSwaySpeed{
        "Game.SpeedGauge.SwaySpeed", 1.8f,
        "蔦が揺れるはやさ",
        CVarRange{ 0.0f, 10.0f } };

    CVar<float> cvFoliageBrightness{
        "Game.SpeedGauge.FoliageBrightness", 0.62f,
        "蔦の明るさ。ブルームで白く飛ぶので下げて使うが、板を濃くしたぶん少し戻してある",
        CVarRange{ 0.05f, 2.0f } };

    /// @brief 板と同じ設定で UIImage を 1 枚生やす
    UIImage* SpawnPart(GameObject* owner, const char* texture, const std::string& name, int sortOrder)
    {
        auto* image = owner->Spawn<UIImage>();
        if (!image) {
            return nullptr;
        }
        image->Initialize(texture, name);
        image->SetSerializeEnabled(false);
        image->SetAnchor(UIAnchor::TopLeft);
        image->SetSortOrder(sortOrder);
        return image;
    }

    /// @brief 数字・記号用の UIText を 1 枚生やす
    UIText* SpawnLabel(GameObject* owner, MsdfFont* font, const std::string& textUtf8,
        const std::string& name, float fontSize, const Vector4& color, int sortOrder)
    {
        auto* text = owner->Spawn<UIText>();
        if (!text) {
            return nullptr;
        }
        text->Initialize(font, textUtf8, name);
        text->SetSerializeEnabled(false);
        text->SetAnchor(UIAnchor::TopLeft);
        text->SetPivot({ 0.5f, 0.5f });
        text->SetFontSize(fontSize);
        text->SetColor(color);
        text->SetOutline(kOutlineColor, kOutlineWidth);
        text->SetSortOrder(sortOrder);
        return text;
    }

    /// @brief 送り出しの緩急。止まり際をなめらかにする
    float RollEase(float t)
    {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - clamped) * (1.0f - clamped);
    }

    /// @brief current から target へ、1 フレームぶんの上限 maxDelta まで近づける
    float MoveTowards(float current, float target, float maxDelta)
    {
        const float diff = target - current;
        if (std::abs(diff) <= maxDelta) {
            return target;
        }
        return current + (diff > 0.0f ? maxDelta : -maxDelta);
    }
}

/// @note 上限を 2 まで許すのは、EaseOutBack を通した「行き過ぎ」をそのまま活かすため
void GameComponents::SpeedGaugeUIComponent::SetIntroReveal(float reveal)
{
    introReveal_ = std::clamp(reveal, 0.0f, 2.0f);
}

void GameComponents::SpeedGaugeUIComponent::Awake()
{
    if (!train_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "SpeedGaugeUIComponent: TrainMovement が未設定です");
        SetEnabled(false);
        return;
    }
    BuildParts();
}

void GameComponents::SpeedGaugeUIComponent::BuildParts()
{
    auto* owner = GetOwner();
    if (!owner) {
        return;
    }

    // オーナー自身が中板。横方向に一様なテクスチャなので、伸ばしてもドットが崩れない
    board_ = dynamic_cast<UIImage*>(owner);
    if (!board_) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "SpeedGaugeUIComponent: UIImage にアタッチしてください");
        SetEnabled(false);
        return;
    }

    const int baseOrder = cvSortOrder.Get();
    board_->SetAnchor(UIAnchor::TopLeft);
    board_->SetPivot({ 0.0f, 0.0f });
    board_->SetSortOrder(baseOrder);

    capLeft_ = SpawnPart(owner, kTexBoardCapL, "SpeedGaugeCapL", baseOrder + 1);
    capRight_ = SpawnPart(owner, kTexBoardCapR, "SpeedGaugeCapR", baseOrder + 1);
    for (auto* cap : { capLeft_, capRight_ }) {
        if (cap) {
            cap->SetPivot({ 0.0f, 0.0f });
            cap->SetSize({ kCapWidth, kPanelHeight });
        }
    }

    // 板の縁へ絡ませる蔦。スタミナゲージと同じ本数・同じ揺らし方にしてある
    vines_.reserve(kVineCount);
    for (std::size_t i = 0; i < kVineCount; ++i) {
        auto* vine = SpawnPart(
            owner, kTexVine, "SpeedGaugeVine_" + std::to_string(i), baseOrder + 2);
        if (vine) {
            vine->SetPivot({ 0.5f, 0.5f });
            vine->SetSize({ kVineWidth, kVineHeight });
            vines_.push_back(vine);
        }
    }

    auto* engine = owner->GetEngineSystem();
    auto* fontManager = engine ? engine->GetService<FontManager>() : nullptr;
    if (!fontManager) {
        Logger::GetInstance().Warnf(
            LogCategory::Game,
            "SpeedGaugeUIComponent: FontManager が無いため数字を出しません");
        built_ = true;
        return;
    }

    // 数字はドット感のあるフォントで打つ（MapView の距離表示と同じもの）
    MsdfFontDesc digitFontDesc;
    digitFontDesc.filePath = L"Engine/Assets/font/x8y12pxDenkiChip.ttf";
    digitFontDesc.systemFamilyNames = { L"Segoe UI" };
    digitFontDesc.charsetUtf8 = "0123456789.";
    auto* digitFont = fontManager->Acquire(digitFontDesc);

    // 単位はスタミナゲージの見出しと同じフォントで揃える
    MsdfFontDesc unitFontDesc;
    unitFontDesc.filePath = L"Engine/Assets/font/851Gkktt_005.ttf";
    unitFontDesc.systemFamilyNames = { L"Yu Gothic UI", L"Meiryo", L"Segoe UI" };
    unitFontDesc.charsetUtf8 = "km/h";
    auto* unitFont = fontManager->Acquire(unitFontDesc);

    if (digitFont) {
        for (std::size_t i = 0; i < kDigitCount; ++i) {
            const std::string suffix = std::to_string(i);
            digits_[i].current = SpawnLabel(
                owner, digitFont, "0", "SpeedGaugeDigit_" + suffix,
                kDigitFontSize, kDigitColor, baseOrder + 4);
            digits_[i].outgoing = SpawnLabel(
                owner, digitFont, "0", "SpeedGaugeDigitOut_" + suffix,
                kDigitFontSize, kDigitColor, baseOrder + 3);
            if (digits_[i].outgoing) {
                digits_[i].outgoing->SetActive(false);
            }
        }
        dot_ = SpawnLabel(
            owner, digitFont, ".", "SpeedGaugeDot",
            kDigitFontSize, kDotColor, baseOrder + 4);
    }
    if (unitFont) {
        unit_ = SpawnLabel(
            owner, unitFont, "km/h", "SpeedGaugeUnit",
            kUnitFontSize, kUnitColor, baseOrder + 4);
        if (unit_) {
            unit_->SetPivot({ 0.0f, 0.5f });
        }
    }

    built_ = true;
}

float GameComponents::SpeedGaugeUIComponent::CalculateKilometersPerHour() const
{
    if (!train_) {
        return 0.0f;
    }
    // 速度はマス/秒。1 マスの実距離を掛けて m/s にしてから km/h へ直す
    return train_->GetMoveSpeed() * std::max(0.0f, cvMetersPerCell.Get()) * 3.6f;
}

bool GameComponents::SpeedGaugeUIComponent::UpdateTrainMovingState()
{
    if (!train_) {
        return false;
    }

    const Vector3 position = train_->GetWorldPosition();
    if (!hasTrainPosition_) {
        lastTrainPosition_ = position;
        hasTrainPosition_ = true;
        return false;
    }

    // 投石ジャンプで y だけ動くことがあるので、水平方向だけを見る
    const float dx = position.x - lastTrainPosition_.x;
    const float dz = position.z - lastTrainPosition_.z;
    lastTrainPosition_ = position;
    return (dx * dx + dz * dz) > kMovingEpsilonSquared;
}

void GameComponents::SpeedGaugeUIComponent::Update()
{
    if (!built_ || !board_) {
        return;
    }

    const bool enabled = cvEnabled.Get();
    if (board_->IsActive() != enabled) {
        board_->SetActive(enabled);
        for (auto* part : { capLeft_, capRight_ }) {
            if (part) {
                part->SetActive(enabled);
            }
        }
        for (auto* vine : vines_) {
            if (vine) {
                vine->SetActive(enabled);
            }
        }
        for (auto& digit : digits_) {
            if (digit.current) {
                digit.current->SetActive(enabled);
            }
            // 送り出し中に消すと数字が取り残されるので、こちらは落とすときだけ触る
            if (digit.outgoing && !enabled) {
                digit.outgoing->SetActive(false);
            }
        }
        for (auto* text : { dot_, unit_ }) {
            if (text) {
                text->SetActive(enabled);
            }
        }
    }
    if (!enabled) {
        return;
    }

    const float deltaTime = Time::UnscaledDeltaTime();
    elapsed_ += deltaTime;

    // 発車前と停止中は 0。走り出したら 0 から本来の速度まで一気に振り切る
    const bool moving = UpdateTrainMovingState();
    const float target = moving ? CalculateKilometersPerHour() : 0.0f;
    const float rate = (target > displayedKilometersPerHour_) ? kRiseRate : kFallRate;
    displayedKilometersPerHour_ =
        MoveTowards(displayedKilometersPerHour_, target, rate * deltaTime);

    UpdateDigits(displayedKilometersPerHour_, deltaTime);
    LayoutParts(elapsed_);
}

void GameComponents::SpeedGaugeUIComponent::UpdateDigits(float kilometersPerHour, float deltaTime)
{
    // 小数 1 桁まで見せるので、10 倍した整数にしてから桁をばらす
    const float shown = std::clamp(kilometersPerHour, 0.0f, kMaxDisplayKmh);
    int scaled = static_cast<int>(shown * 10.0f + 0.5f);

    int wanted[kDigitCount]{};
    for (std::size_t i = 0; i < kDigitCount; ++i) {
        // 添字 0 が百の位なので、小さい桁から詰めて逆順に入れる
        wanted[kDigitCount - 1 - i] = scaled % 10;
        scaled /= 10;
    }

    for (std::size_t i = 0; i < kDigitCount; ++i) {
        Digit& digit = digits_[i];
        if (!digit.current) {
            continue;
        }

        if (digit.value != wanted[i]) {
            // 9 から 0 へ繰り上がるときも、見た目は上へ送りたい
            const bool increased = digit.value < 0 ||
                (wanted[i] > digit.value ? (wanted[i] - digit.value) <= 5
                                         : (digit.value - wanted[i]) > 5);
            digit.direction = increased ? 1.0f : -1.0f;
            digit.outgoingValue = digit.value;
            digit.value = wanted[i];
            digit.roll = (digit.outgoingValue < 0) ? 1.0f : 0.0f;
            digit.current->SetText(std::to_string(digit.value));
            if (digit.outgoing) {
                const bool showsOutgoing = digit.outgoingValue >= 0;
                digit.outgoing->SetActive(showsOutgoing);
                if (showsOutgoing) {
                    digit.outgoing->SetText(std::to_string(digit.outgoingValue));
                }
            }
        }

        if (digit.roll < 1.0f) {
            digit.roll = std::min(1.0f, digit.roll + deltaTime / kRollSeconds);
            if (digit.roll >= 1.0f && digit.outgoing) {
                digit.outgoing->SetActive(false);
            }
        }
    }
}

void GameComponents::SpeedGaugeUIComponent::LayoutParts(float time)
{
    const float scale = cvScale.Get();
    const float panelH = kPanelHeight * scale;
    const float capW = kCapWidth * scale;
    const float padding = kInnerPadding * scale;
    const float panelWidth = capW * 2.0f + padding * 2.0f + kContentWidth * scale;

    // スタミナゲージと同じ TopLeft アンカー。左上からの距離をそのまま使う。
    // 突入演出あけの登場では、板の右端が画面外へ抜ける距離まで左へ寄せてから戻す。
    const Vector2 basePosition = cvPosition.Get();
    const Vector2 origin{
        basePosition.x - (1.0f - introReveal_) * (basePosition.x + panelWidth + kIntroMargin),
        basePosition.y };

    // 板と端木は濃いめに落として緑へ寄せる。湿ったジャングルの木らしい色みにする
    const float brightness = cvBoardBrightness.Get();
    const float greenTint = cvBoardGreenTint.Get();
    const Vector4 boardColor{
        brightness * (1.0f - greenTint * 0.55f),
        brightness * (1.0f + greenTint * 0.45f),
        brightness * (1.0f - greenTint * 0.95f),
        1.0f };

    board_->SetAnchoredPosition({ origin.x + capW, origin.y });
    board_->SetSize({ panelWidth - capW * 2.0f, panelH });
    board_->SetColor(boardColor);

    if (capLeft_) {
        capLeft_->SetAnchoredPosition({ origin.x, origin.y });
        capLeft_->SetSize({ capW, panelH });
        capLeft_->SetColor(boardColor);
    }
    if (capRight_) {
        capRight_->SetAnchoredPosition({ origin.x + panelWidth - capW, origin.y });
        capRight_->SetSize({ capW, panelH });
        capRight_->SetColor(boardColor);
    }

    // 蔦はスタミナゲージと同じ式で並べ、同じ速さで揺らす
    const float swaySpeed = cvSwaySpeed.Get();
    const float foliage = cvFoliageBrightness.Get();
    const Vector4 foliageColor{ foliage, foliage, foliage, 1.0f };
    const float vineW = kVineWidth * scale;
    const float vineH = kVineHeight * scale;
    const float vineOverhang = kVineOverhang * scale;
    const float inner = panelWidth - capW * 2.0f;
    for (std::size_t i = 0; i < vines_.size(); ++i) {
        auto* vine = vines_[i];
        if (!vine) {
            continue;
        }
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(vines_.size());
        const bool onTop = (i % 2) == 0;
        const float centerY = onTop
            ? origin.y - vineOverhang + vineH * 0.5f
            : origin.y + panelH + vineOverhang - vineH * 0.5f;
        const float phase = static_cast<float>(i) * 1.1f;
        vine->SetSize({ vineW, vineH });
        vine->SetAnchoredPosition({ origin.x + capW + inner * t, centerY });
        vine->SetUIRotation(
            (onTop ? 0.0f : MathCore::Constants::kPi) +
            std::sin(time * swaySpeed * 0.45f + phase) * 0.05f);
        vine->SetColor(foliageColor);
    }

    // 数字・小数点・単位を左から順に置く
    const float digitW = kDigitWidth * scale;
    const float digitGap = kDigitGap * scale;
    const float dotW = kDotWidth * scale;
    const float dotSideGap = kDotSideGap * scale;
    const float travel = kRollTravel * scale;
    const float centerY = origin.y + panelH * 0.5f;

    float cursor = origin.x + capW + padding;
    for (std::size_t i = 0; i < kDigitCount; ++i) {
        // 整数 3 桁のあとに小数点を挟む
        if (i == kDigitCount - 1) {
            if (dot_) {
                dot_->SetFontSize(kDigitFontSize * scale);
                dot_->SetAnchoredPosition({
                    cursor + dotSideGap + dotW * 0.5f,
                    centerY + kDotBaselineOffset * scale });
            }
            cursor += dotSideGap + dotW + dotSideGap;
        }

        Digit& digit = digits_[i];
        const float digitCenterX = cursor + digitW * 0.5f;
        const float eased = RollEase(digit.roll);

        // 縁取りも本体と同じ濃さで薄める。片方だけ残ると輪郭だけが浮いて見える
        if (digit.current) {
            digit.current->SetFontSize(kDigitFontSize * scale);
            digit.current->SetAnchoredPosition({
                digitCenterX, centerY + (1.0f - eased) * travel * digit.direction });
            digit.current->SetColor({
                kDigitColor.x, kDigitColor.y, kDigitColor.z, eased });
            digit.current->SetOutline({
                kOutlineColor.x, kOutlineColor.y, kOutlineColor.z, eased }, kOutlineWidth);
        }
        if (digit.outgoing && digit.outgoing->IsActive()) {
            const float fade = 1.0f - eased;
            digit.outgoing->SetFontSize(kDigitFontSize * scale);
            digit.outgoing->SetAnchoredPosition({
                digitCenterX, centerY - eased * travel * digit.direction });
            digit.outgoing->SetColor({
                kDigitColor.x, kDigitColor.y, kDigitColor.z, fade });
            digit.outgoing->SetOutline({
                kOutlineColor.x, kOutlineColor.y, kOutlineColor.z, fade }, kOutlineWidth);
        }

        cursor += digitW;
        if (i + 2 < kDigitCount) {
            cursor += digitGap;
        }
    }

    if (unit_) {
        unit_->SetFontSize(kUnitFontSize * scale);
        unit_->SetAnchoredPosition({ cursor + kUnitGap * scale, centerY });
    }
}

#ifdef USE_IMGUI
bool GameComponents::SpeedGaugeUIComponent::DrawInspector()
{
    const bool changed = CVarUI::DrawTree("Game.SpeedGauge");
    UI::Hint("変更は CVars.json へ自動保存されます。");
    ImGui::Separator();
    ImGui::Text("表示: %.1f km/h", displayedKilometersPerHour_);
    if (train_) {
        ImGui::TextDisabled(
            "速度: %.3f マス/秒（最低 %.3f）",
            train_->GetMoveSpeed(), train_->GetMinMoveSpeed());
    }
    return changed;
}
#endif

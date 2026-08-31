#include "pch.h"
#include "MsdfTextTestScene.h"

// BaseScene が unique_ptr で持つ型。デストラクタを .cpp 側で定義するため実体が要る
#include "Camera/CameraManager.h"

#include "EngineSystem/EngineSystem.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Text/MsdfFont.h"
#include "UI/UIImage.h"
#include "UI/UIText.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <cmath>
#include <numbers>
#include <string>

namespace MsdfTextTest
{
    using namespace CoreEngine;

    namespace
    {
        /// 1x1 の白テクスチャ。scale がそのままピクセルサイズになるので矩形として使える
        constexpr const char* kWhiteTexture = "white1x1.png";

        // ── 検証に使う文字列 ──────────────────────────────────────
        // ソースは /utf-8 でコンパイルされるので、素の文字列リテラルが UTF-8 になる
        constexpr const char* kTitleText = "MSDF フォント描画テスト";
        constexpr const char* kLadderText = "あアA亜g国 0123 永";
        constexpr const char* kScalingText = "拡大テスト Ag亜";
        constexpr const char* kRotatingText = "回転しても鋭い";
        constexpr const char* kCornerText = "角が丸まらない：レ・ト・国・永";

        /// サイズ階段（px）。全て同じ 1 枚のアトラスから描かれる
        constexpr float kLadderSizes[] = { 12.0f, 16.0f, 22.0f, 30.0f, 42.0f, 58.0f };

        /// 拡大縮小アニメーションの範囲（px）
        constexpr float kScaleMinPx = 12.0f;
        constexpr float kScaleMaxPx = 100.0f;
        constexpr float kScalePeriodSeconds = 6.0f;

        /// アトラス生成に使うシステムフォントの候補（上から順に試す）
        const std::vector<std::wstring> kFontCandidates = {
            L"Yu Gothic UI",
            L"Meiryo",
            L"MS Gothic",
            L"Segoe UI",
        };

        /// アトラスの目視確認用 PNG の出力先（作業ディレクトリは Project/）
        constexpr const char* kAtlasDumpPath = "Cache/FontCache/MsdfTextTest_atlas.png";
    }

    MsdfTextTestScene::MsdfTextTestScene() = default;
    MsdfTextTestScene::~MsdfTextTestScene() = default;

    void MsdfTextTestScene::OnInitialize()
    {
        SetSceneName("MsdfTextTestScene");

        // 3D の床や空が映り込むと文字の輪郭が見づらいので止める
        SetDefaultGroundEnabled(false);

        BuildFont();

        // ── 背景（暗色。白文字のコントラストを取る）──────────────
        {
            auto* background = CreateObject<UIImage>();
            background->Initialize(kWhiteTexture, "Background");
            background->SetSerializeEnabled(false);
            background->SetAnchor(UIAnchor::Center);
            background->SetPivot({ 0.5f, 0.5f });
            background->SetAnchoredPosition({ 0.0f, 0.0f });
            background->SetSize({ 4096.0f, 4096.0f });
            background->SetColor({ 0.09f, 0.10f, 0.13f, 1.0f });
            background->SetSortOrder(-100);
        }

        if (!font_ || !font_->IsValid()) {
            // フォントが用意できなくてもシーンは起動する（背景だけ出る）
            return;
        }

        const Vector4 white = { 0.95f, 0.96f, 0.98f, 1.0f };
        const Vector4 accent = { 0.98f, 0.78f, 0.30f, 1.0f };
        const Vector4 dim = { 0.55f, 0.60f, 0.68f, 1.0f };

        // ── 見出し ────────────────────────────────────────────────
        CreateText(kTitleText, 34.0f, { 40.0f, 28.0f }, white, "Title");

        {
            // どのフォントが採用されたか・どう焼いたかを画面から読めるようにする
            const std::wstring& fontName = font_->GetResolvedFontName();
            const std::string info =
                "font: " + Logger::GetInstance().PathToUtf8(fontName)
                + "  /  atlas: " + std::to_string(static_cast<int>(font_->GetAtlasSize().x))
                + "x" + std::to_string(static_cast<int>(font_->GetAtlasSize().y))
                + "  /  pxRange: " + std::to_string(static_cast<int>(font_->GetPxRange()));
            CreateText(info, 15.0f, { 40.0f, 74.0f }, dim, "FontInfo");
        }

        // ── ①サイズ階段：1 枚のアトラスから 12px と 58px を同時に出す ──
        {
            float y = 118.0f;
            for (float size : kLadderSizes) {
                const std::string label =
                    std::string(kLadderText) + "  " + std::to_string(static_cast<int>(size)) + "px";
                CreateText(label, size, { 40.0f, y }, white, "Ladder");
                y += size + 14.0f;
            }
        }

        // ── ②コーナー保存の確認 ──────────────────────────────────
        // SDF なら丸まる字形（カタカナの角・漢字の交差）を並べる
        CreateText(kCornerText, 40.0f, { 40.0f, 430.0f }, accent, "CornerCheck");

        // ── ③拡大縮小アニメーション（要件の直接検証）──────────────
        scalingText_ = CreateText(
            kScalingText, kScaleMinPx, { 0.0f, 90.0f }, accent, "ScalingText");
        if (scalingText_) {
            scalingText_->SetAnchor(UIAnchor::Center);
            scalingText_->SetPivot({ 0.5f, 0.5f });
        }

        // ── ④回転しても崩れないことの確認 ────────────────────────
        rotatingText_ = CreateText(
            kRotatingText, 30.0f, { 0.0f, 220.0f }, white, "RotatingText");
        if (rotatingText_) {
            rotatingText_->SetAnchor(UIAnchor::Center);
            rotatingText_->SetPivot({ 0.5f, 0.5f });
        }

        // ── 見方の説明 ────────────────────────────────────────────
        {
            auto* hint = CreateText(
                "拡大しても輪郭が鋭いまま／縮小しても消えないことを確認する",
                16.0f, { 40.0f, -34.0f }, dim, "Hint");
            if (hint) {
                hint->SetAnchor(UIAnchor::BottomLeft);
                hint->SetPivot({ 0.0f, 0.0f });
            }
        }
    }

    void MsdfTextTestScene::BuildFont()
    {
        auto* graphicsCore = engine_ ? engine_->GetService<GraphicsCore>() : nullptr;
        if (!graphicsCore) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfTextTestScene: GraphicsCore を取得できませんでした");
            return;
        }

        MsdfFontDesc desc{};
        desc.systemFamilyNames = kFontCandidates;

        // 最小構成では「使う文字を全部先に焼く」方式。
        // 和文を本格対応する際は、ここを動的アトラス（出てきた文字だけ焼く）へ置き換える
        desc.charsetUtf8 =
            std::string(kTitleText) + kLadderText + kScalingText + kRotatingText + kCornerText
            + "px 拡大しても輪郭が鋭いまま／縮小しても消えないことを確認する"
            + "font atlas pxRange";
        desc.includeAscii = true;

        desc.bake.glyphPixelSize = 40; // 和文の推奨値
        desc.bake.pxRange = 4.0f;      // 40px 焼きでの安全圏
        desc.bake.atlasWidth = 1024;
        desc.bake.atlasHeight = 1024;
        desc.bake.padding = 2;

        // 工程①（DirectWrite → Shape 変換）の目視確認用。
        // 出力された PNG を開いて、グリフが正しい向き・形で焼けているかを見る
        desc.debugAtlasDumpPath = kAtlasDumpPath;

        font_ = std::make_unique<MsdfFont>();
        if (!font_->Build(graphicsCore, desc)) {
            Logger::GetInstance().Logf(LogLevel::Error, LogCategory::Resource,
                "MsdfTextTestScene: MSDF フォントの構築に失敗しました");
            font_.reset();
        }
    }

    UIText* MsdfTextTestScene::CreateText(
        const std::string& text,
        float fontSize,
        const Vector2& position,
        const Vector4& color,
        const std::string& name)
    {
        if (!font_) { return nullptr; }

        auto* uiText = CreateObject<UIText>();
        uiText->Initialize(font_.get(), text, name);
        uiText->SetSerializeEnabled(false);
        uiText->SetAnchor(UIAnchor::TopLeft);
        uiText->SetPivot({ 0.0f, 0.0f });
        uiText->SetAnchoredPosition(position);
        uiText->SetFontSize(fontSize);
        uiText->SetColor(color);
        uiText->SetSortOrder(10);
        return uiText;
    }

    void MsdfTextTestScene::OnUpdate()
    {
        elapsedSeconds_ += Time::DeltaTime();

        // ── 拡大縮小：0..1 を往復させてフォントサイズへ写す ────────
        // 頂点は em 単位なので、ここでサイズを変えても頂点は組み直されない。
        // それでも輪郭が鋭いままなのが MSDF の効果
        if (scalingText_) {
            const float phase =
                elapsedSeconds_ * 2.0f * static_cast<float>(std::numbers::pi) / kScalePeriodSeconds;
            const float t = 0.5f - 0.5f * std::cos(phase); // 0..1 を滑らかに往復
            scalingText_->SetFontSize(kScaleMinPx + (kScaleMaxPx - kScaleMinPx) * t);
        }

        // ── 回転：MSDF は回転にも強い（ビットマップ方式だと斜めでジャギる）──
        if (rotatingText_) {
            rotatingText_->SetUIRotation(std::sin(elapsedSeconds_ * 0.7f) * 0.35f);
        }
    }
}

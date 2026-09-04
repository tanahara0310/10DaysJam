#include "pch.h"
#include "Text3DTestScene.h"

#include "EngineSystem/EngineSystem.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "GameObject/Text3D/Text3DObject.h"
#include "Graphics/Primitive/SphereMeshGenerator.h"
#include "Editor/Environment/AtmosphereEditor.h"
#include "Utility/FrameRate/Time.h"

#include <cmath>
#include <numbers>

using namespace CoreEngine::MathCore;

namespace
{
    constexpr float kPi = std::numbers::pi_v<float>;
    constexpr float kDeg = kPi / 180.0f;

    /// カメラが見る中心と、そこからの距離・高さ
    constexpr CoreEngine::Vector3 kPivot = { 0.0f, 2.6f, 0.0f };
    constexpr float kCameraRadius = 11.0f;
    constexpr float kCameraHeight = 6.0f;
    constexpr float kCameraFovDegrees = 50.0f;

    /// 左右の振れ幅と速さ。ビルボードの追従を見分けるためだけのゆっくりした動き
    constexpr float kYawSweep = 12.0f * kDeg;
    constexpr float kYawSpeed = 0.55f;

    /// 3 つのクラスタを並べる X 座標と、見出しの高さ
    constexpr float kDepthX = -5.8f;
    constexpr float kBillboardX = 0.0f;
    constexpr float kLayoutX = 5.0f;
    constexpr float kCaptionY = 5.8f;

    // よく使う色
    constexpr CoreEngine::Vector4 kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr CoreEngine::Vector4 kBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr CoreEngine::Vector4 kAmber = { 1.0f, 0.78f, 0.25f, 1.0f };
    constexpr CoreEngine::Vector4 kCyan = { 0.45f, 0.90f, 1.0f, 1.0f };
    constexpr CoreEngine::Vector4 kLime = { 0.65f, 1.0f, 0.45f, 1.0f };
    constexpr CoreEngine::Vector4 kRose = { 1.0f, 0.45f, 0.50f, 1.0f };
}

namespace CoreEngine
{
    void Text3DTestScene::OnInitialize()
    {
        SetSceneName("Text3DTestScene");
        SetReleaseCameraLens(kCameraFovDegrees, 500.0f, 0.1f);

        // 文字が沈まない程度に太陽を入れる（球の陰影が出ないと遮蔽が読み取りにくい）
        if (Light* sun = GetDirectionalLight()) {
            sun->direction = AtmosphereEditor::ComputeSunLightDirection(40.0f, 20.0f);
            sun->atmosphereIntensity = 20.0f;
            sun->intensity = kAtmosphereSunIlluminanceLux;
        }

        BuildDepthCluster();
        BuildBillboardCluster();
        BuildLayoutCluster();
        BuildScaleLadder();
        BuildStyleRow();
        BuildGroundText();

        // 初期フレームから構図が決まるよう、Update と同じ式で 1 回置いておく
        OnUpdate();
    }

    Text3DObject* Text3DTestScene::CreateText(const std::string& name,
        const std::string& textUtf8,
        const Vector3& position,
        float fontSize)
    {
        auto* text = CreateObject<Text3DObject>();
        text->SetName(name);
        text->SetText(textUtf8);
        text->SetFontSize(fontSize);

        // 背景（空・床・球）が明るいので、既定で黒縁を付けて可読性を確保する。
        // 縁取りは距離場のしきい値をずらすだけなので、追加のアトラスもドローコールも要らない
        text->SetOutline(kBlack, 0.055f);

        if (auto* transform = text->GetComponent<TransformComponent>()) {
            transform->Get().translate = position;
        }
        return text;
    }

    Text3DObject* Text3DTestScene::CreateCaption(const std::string& name,
        const std::string& textUtf8, float x)
    {
        auto* caption = CreateText(name, textUtf8, { x, kCaptionY, 1.0f }, 0.40f);
        caption->SetBillboard(Text3DBillboard::ViewFacing);
        caption->SetColor(kCyan);
        return caption;
    }

    void Text3DTestScene::CreateOccluderSphere(const Vector3& position, float radius)
    {
        auto* sphere = CreateObject("Occluder");
        sphere->AddComponent<MeshRendererComponent>(
            std::make_unique<SphereMeshGenerator>(radius));

        auto& transform = sphere->GetComponent<TransformComponent>()->Get();
        transform.translate = position;

        auto* material = sphere->AddComponent<MaterialComponent>();
        material->SetPBR(0.0f, 0.45f, 1.0f);
        material->SetIBLIntensity(1.0f);

        sphere->SetActive(true);
    }

    void Text3DTestScene::BuildDepthCluster()
    {
        // 球を手前に、文字を少し奥に置く。
        // 深度テストが効いていれば上段は球に食われ、オーバーレイの下段は貫通して見える
        CreateOccluderSphere({ kDepthX, 3.0f, 1.4f }, 1.25f);

        auto* occluded = CreateText("Text3D_Depth_Test",
            "Test：遮蔽される", { kDepthX, 3.7f, 3.0f }, 0.40f);
        occluded->SetBillboard(Text3DBillboard::ViewFacing);
        occluded->SetDepthMode(Text3DDepthMode::Test);
        occluded->SetColor(kWhite);

        auto* overlay = CreateText("Text3D_Depth_Overlay",
            "Overlay：壁越し", { kDepthX, 2.3f, 3.0f }, 0.40f);
        overlay->SetBillboard(Text3DBillboard::ViewFacing);
        overlay->SetDepthMode(Text3DDepthMode::Overlay);
        overlay->SetColor(kAmber);

        CreateCaption("Text3D_Depth_Caption", "― 深度モード ―", kDepthX);
    }

    void Text3DTestScene::BuildBillboardCluster()
    {
        // 3 つとも同じ Y 回転を与える。
        // None だけが回転に従い、残り 2 つはカメラ基準の姿勢で上書きされる
        constexpr float kYaw = 40.0f * kDeg;

        struct Entry
        {
            const char* name;
            const char* label;
            Text3DBillboard mode;
            Vector4 color;
            float y;
        };

        const Entry entries[] = {
            { "Text3D_BB_None",       "None（回転が効く）", Text3DBillboard::None,       kWhite, 4.6f },
            { "Text3D_BB_ViewFacing", "ViewFacing",         Text3DBillboard::ViewFacing, kLime,  3.3f },
            { "Text3D_BB_YAxisOnly",  "YAxisOnly",          Text3DBillboard::YAxisOnly,  kRose,  2.0f },
        };

        for (const Entry& entry : entries) {
            auto* text = CreateText(entry.name, entry.label,
                { kBillboardX, entry.y, 1.5f }, 0.40f);
            text->SetBillboard(entry.mode);
            text->SetColor(entry.color);

            // ビルボードが有効なものはここで与えた回転を無視する（それが確認したい挙動）
            if (auto* transform = text->GetComponent<TransformComponent>()) {
                transform->Get().rotate = { 0.0f, kYaw, 0.0f };
            }
        }

        CreateCaption("Text3D_BB_Caption",
            "― ビルボード（3つとも Y 回転 40°）―", kBillboardX);
    }

    void Text3DTestScene::BuildLayoutCluster()
    {
        // 折り返し幅を指定した和文。句読点が行頭に来ないこと（禁則）と、
        // 欧文が単語の途中で切れないことを見る
        auto* wrapped = CreateText("Text3D_Layout_Wrap",
            "日本語の折り返しと禁則処理、"
            "それに alphabet words の分かち書きを"
            "確認するための長めの文字列です。",
            { kLayoutX, 3.4f, 2.0f }, 0.30f);
        wrapped->SetBillboard(Text3DBillboard::ViewFacing);
        wrapped->SetWrapWidth(3.6f);           // ワールド単位で折る
        wrapped->SetAlign(TextAlignH::Left, TextAlignV::Top);
        wrapped->SetLineSpacing(1.2f);
        wrapped->SetColor(kWhite);

        CreateCaption("Text3D_Layout_Caption", "― 折り返し・禁則 ―", kLayoutX);
    }

    void Text3DTestScene::BuildScaleLadder()
    {
        // 同じ字を 4 段階の大きさで。頂点は em 単位のままなので、
        // 大きさを変えても組み直しは起きないし、輪郭も鈍らない
        constexpr float kSizes[] = { 0.20f, 0.34f, 0.56f, 0.90f };
        float x = -7.2f;

        for (float size : kSizes) {
            auto* text = CreateText("Text3D_Scale", "鋭Aa", { x, 1.3f, -2.0f }, size);
            text->SetBillboard(Text3DBillboard::ViewFacing);
            text->SetColor(kWhite);
            x += size * 3.2f + 0.7f;
        }

        auto* caption = CreateText("Text3D_Scale_Caption",
            "― 大きさ違い（距離場なので輪郭は鋭いまま）―",
            { -4.4f, 0.35f, -2.0f }, 0.28f);
        caption->SetBillboard(Text3DBillboard::ViewFacing);
        caption->SetColor(kCyan);
    }

    void Text3DTestScene::BuildStyleRow()
    {
        // 縁取りと太さはしきい値をずらすだけなので、
        // アトラスを焼き直さずにテキストごとへ自由に与えられる
        struct Entry
        {
            const char* label;
            float outlineEm;
            float weightEm;
            Vector4 color;
        };

        const Entry entries[] = {
            { "縁なし",   0.000f,  0.000f, kWhite },
            { "縁 0.10",  0.100f,  0.000f, kAmber },
            { "太さ +",   0.045f,  0.020f, kLime  },
            { "太さ −",   0.045f, -0.015f, kRose  },
        };

        float x = 0.9f;
        for (const Entry& entry : entries) {
            auto* text = CreateText("Text3D_Style", entry.label, { x, 1.3f, -2.0f }, 0.34f);
            text->SetBillboard(Text3DBillboard::ViewFacing);
            text->SetOutline(kBlack, entry.outlineEm);
            text->SetWeight(entry.weightEm);
            text->SetColor(entry.color);
            x += 1.7f;
        }

        auto* caption = CreateText("Text3D_Style_Caption",
            "― 縁取り・太さ ―", { 3.4f, 0.35f, -2.0f }, 0.28f);
        caption->SetBillboard(Text3DBillboard::ViewFacing);
        caption->SetColor(kCyan);
    }

    void Text3DTestScene::BuildGroundText()
    {
        // ビルボードなしなら任意の姿勢を取れる。X 軸まわりに 90° 倒して床へ寝かせる
        auto* ground = CreateText("Text3D_Ground",
            "床に寝かせた文字 GROUND", { -2.6f, 0.02f, 5.5f }, 0.55f);
        ground->SetBillboard(Text3DBillboard::None);
        ground->SetColor(kAmber);
        ground->SetWeight(0.008f);   // 見下ろすと痩せて見えるので少しだけ太らせる

        if (auto* transform = ground->GetComponent<TransformComponent>()) {
            transform->Get().rotate = { 90.0f * kDeg, 0.0f, 0.0f };
        }
    }

    void Text3DTestScene::OnUpdate()
    {
        // ピボットを見ながら左右にゆっくり振れる。
        // ビルボードの追従は静止画では分からないので、角度を変えて確かめるための動き
        const float elapsed = Time::UnscaledTimeSinceStartup();
        const float sweep = kYawSweep * std::sin(elapsed * kYawSpeed);

        const Vector3 eye = {
            kPivot.x - kCameraRadius * std::sin(sweep),
            kCameraHeight,
            kPivot.z - kCameraRadius * std::cos(sweep),
        };

        const Vector3 toPivot = {
            kPivot.x - eye.x,
            kPivot.y - eye.y,
            kPivot.z - eye.z,
        };
        const float horizontal =
            std::sqrt(toPivot.x * toPivot.x + toPivot.z * toPivot.z);

        // 前方 (0,0,1) を X 回転で下へ、Y 回転で横へ向ける向き
        const float pitch = std::atan2(-toPivot.y, horizontal);
        const float yaw = std::atan2(toPivot.x, toPivot.z);

        SetReleaseCameraTransform(eye, { pitch, yaw, 0.0f });
    }
}

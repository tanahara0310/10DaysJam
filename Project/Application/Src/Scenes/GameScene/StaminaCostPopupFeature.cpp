#include "pch.h"
#include "StaminaCostPopupFeature.h"

#include "Camera/Camera.h"
#include "Components/Rail/RailBuilderComponent.h"
#include "Components/UI/PauseMenuUIComponent.h"
#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObjectManager.h"
#include "Math/MathCore.h"
#include "Text/FontManager.h"
#include "UI/UIText.h"
#include "Utility/FrameRate/Time.h"
#include "WinApp/WinApp.h"

#include <algorithm>
#include <array>
#include <format>

using namespace CoreEngine;

namespace
{
    constexpr float kLifetime = 0.9f;
    constexpr float kFadeStart = 0.2f;
    constexpr float kRisePixels = 80.0f;
    constexpr float kHeadOffsetPixels = 44.0f;
    constexpr float kFontSize = 38.0f;
    constexpr float kOutlineWidth = 0.045f;
    // 速度計と同じ黄色・暗い縁取り。
    const Vector4 kColor{ 0.980f, 0.839f, 0.200f, 1.0f };
    const Vector4 kOutlineColor{ 0.031f, 0.020f, 0.012f, 1.0f };

    class StaminaCostPopupFeature final : public ISceneFeature
    {
    public:
        const char* GetName() const override { return "StaminaCostPopup"; }
        // ポーズ中も非表示への切り替えを反映する。寿命はゲーム時間で止める。
        bool RunsWhileStopped() const override { return true; }

        void PostSceneInitialize(SceneContext& ctx) override
        {
            if (!ctx.gameObjectManager || !ctx.engine) {
                return;
            }
            builder_ = ctx.gameObjectManager->FindFirstComponent<GameComponents::RailBuilderComponent>();
            pauseMenu_ = ctx.gameObjectManager->FindFirstComponent<GameComponents::PauseMenuUIComponent>();
            auto* fonts = ctx.engine->GetService<FontManager>();
            if (!builder_ || !fonts) {
                return;
            }

            MsdfFontDesc desc;
            desc.filePath = L"Engine/Assets/font/x8y12pxDenkiChip.ttf";
            desc.systemFamilyNames = { L"Segoe UI" };
            desc.charsetUtf8 = "-0123456789";
            auto* font = fonts->Acquire(desc);
            if (!font) {
                return;
            }

            // 連続敷設でも、各数字が寿命を持って独立に上昇する。
            for (std::size_t i = 0; i < popups_.size(); ++i) {
                auto* text = ctx.gameObjectManager->AddObject(std::make_unique<UIText>());
                if (!text) {
                    continue;
                }
                text->Initialize(font, "", "StaminaCostPopup_" + std::to_string(i));
                text->SetSerializeEnabled(false);
                text->SetAnchor(UIAnchor::TopLeft);
                text->SetPivot({ 0.5f, 0.5f });
                text->SetFontSize(kFontSize);
                text->SetSortOrder(900);
                text->SetActive(false);
                popups_[i].text = text;
            }
            builder_->SetStaminaConsumedFeedback(
                [this](float amount, const Vector3& position) { Show(amount, position); });
        }

        void Update(SceneContext& ctx, SceneUpdatePhase phase) override
        {
            if (phase != SceneUpdatePhase::PostLogic) {
                return;
            }
            const bool paused = pauseMenu_ && pauseMenu_->IsOpen();
            const float dt = paused ? 0.0f : std::max(0.0f, Time::DeltaTime());
            const Camera* camera = ctx.gameViewCamera3D;
            for (auto& popup : popups_) {
                if (!popup.text || popup.age >= kLifetime) {
                    continue;
                }
                popup.age = std::min(kLifetime, popup.age + dt);
                if (paused || !camera || popup.age >= kLifetime) {
                    popup.text->SetActive(false);
                    continue;
                }

                // 発生したマスを毎フレーム射影し、カメラ移動にも追従する。
                const Matrix4x4 viewProjection = camera->GetViewMatrix() * camera->GetProjectionMatrix();
                const Vector4 clip = MathCore::CoordinateTransform::TransformCoord(
                    Vector4{ popup.position.x, popup.position.y, popup.position.z, 1.0f }, viewProjection);
                if (clip.w <= 1.0e-5f || clip.z < 0.0f || clip.z > clip.w) {
                    popup.text->SetActive(false);
                    continue;
                }
                const float x = clip.x / clip.w - camera->GetProjectionJitterX();
                const float y = clip.y / clip.w - camera->GetProjectionJitterY();
                const float progress = popup.age / kLifetime;
                popup.text->SetAnchoredPosition({
                    (x * 0.5f + 0.5f) * static_cast<float>(WinApp::kReferenceWidth),
                    (0.5f - y * 0.5f) * static_cast<float>(WinApp::kReferenceHeight)
                        - kHeadOffsetPixels - kRisePixels * progress });

                const float fade = std::clamp(
                    (popup.age - kFadeStart) / (kLifetime - kFadeStart), 0.0f, 1.0f);
                const float alpha = 1.0f - fade * fade * (3.0f - 2.0f * fade);
                popup.text->SetColor({ kColor.x, kColor.y, kColor.z, alpha });
                popup.text->SetOutline({ kOutlineColor.x, kOutlineColor.y, kOutlineColor.z, alpha },
                    kOutlineWidth);
                popup.text->SetActive(true);
            }
        }

        void Finalize(SceneContext&) override
        {
            if (builder_) {
                builder_->SetStaminaConsumedFeedback(nullptr);
            }
            builder_ = nullptr;
            pauseMenu_ = nullptr;
            popups_ = {};
        }

    private:
        void Show(float amount, const Vector3& position)
        {
            auto slot = std::max_element(popups_.begin(), popups_.end(),
                [](const Popup& a, const Popup& b) {
                    return (a.text ? a.age : -1.0f) < (b.text ? b.age : -1.0f);
                });
            if (!slot->text) {
                return;
            }
            slot->position = position;
            slot->age = 0.0f;
            slot->text->SetText(std::format("-{:.0f}", amount));
        }

        struct Popup {
            UIText* text = nullptr;
            Vector3 position{};
            float age = kLifetime;
        };
        // 最短の敷設間隔（0.01秒）でも、0.9秒の寿命を最後まで再生できる。
        std::array<Popup, 96> popups_{};
        GameComponents::RailBuilderComponent* builder_ = nullptr;
        GameComponents::PauseMenuUIComponent* pauseMenu_ = nullptr;
    };
}

std::unique_ptr<ISceneFeature> GameComponents::CreateStaminaCostPopupFeature()
{
    return std::make_unique<StaminaCostPopupFeature>();
}

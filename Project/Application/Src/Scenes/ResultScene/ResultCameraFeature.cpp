#include "pch.h"
#include "ResultCameraFeature.h"

#include "Camera/Camera.h"
#include "Camera/CameraManager.h"
#include "GameObject/GameObject.h"
#include "GameObject/GameObjectManager.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Utility/CVar/CVar.h"
#include "Utility/FrameRate/Time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace GameComponents
{
    namespace
    {
        using namespace CoreEngine;

        CVar<float> OrbitDuration{ "Result.Camera.OrbitDuration", 8.0f,
            "サルの周囲をゆっくり回る時間（秒）", CVarRange{ 0.1f, 60.0f } };
        CVar<float> FixedDuration{ "Result.Camera.FixedDuration", 6.0f,
            "固定位置から手振れを加える時間（秒）", CVarRange{ 0.1f, 60.0f } };
        CVar<float> ApproachDuration{ "Result.Camera.ApproachDuration", 7.0f,
            "サルへ近づく時間（秒）", CVarRange{ 0.1f, 60.0f } };
        CVar<float> BlendDuration{ "Result.Camera.BlendDuration", 1.2f,
            "カメラワークの切り替えをなじませる時間（秒）", CVarRange{ 0.0f, 5.0f } };
        CVar<Vector3> TargetOffset{ "Result.Camera.TargetOffset", { 0.0f, 0.8f, 0.0f },
            "サルの位置からの注視点オフセット", CVarRange{ -20.0f, 20.0f } };
        CVar<float> OrbitRadius{ "Result.Camera.OrbitRadius", 9.0f,
            "周回カメラの半径", CVarRange{ 1.0f, 100.0f } };
        CVar<float> Height{ "Result.Camera.Height", 3.2f,
            "注視点からのカメラの高さ", CVarRange{ 0.1f, 30.0f } };
        CVar<float> OrbitAngle{ "Result.Camera.OrbitAngle", 50.0f,
            "周回中に回る角度（度）", CVarRange{ 0.0f, 120.0f } };
        CVar<float> ApproachRatio{ "Result.Camera.ApproachRatio", 0.72f,
            "接近後の距離倍率", CVarRange{ 0.3f, 1.0f } };
        CVar<float> ShakeStrength{ "Result.Camera.ShakeStrength", 0.035f,
            "固定カメラの手振れ幅（0で無効）", CVarRange{ 0.0f, 0.3f } };
        CVar<float> ShakeSpeed{ "Result.Camera.ShakeSpeed", 1.0f,
            "固定カメラの手振れ速度", CVarRange{ 0.0f, 5.0f } };

        float SmoothStep(float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        struct CameraPose
        {
            Vector3 offset;
            Vector3 aimOffset;
        };

        CameraPose EvaluatePose(size_t shot, float progress, float seconds)
        {
            const float sweep = OrbitAngle.Get() * std::numbers::pi_v<float> / 180.0f;
            const float angle = sweep * (shot == 0 ? progress - 0.5f : 0.5f);
            const float distanceScale = shot == 2
                ? 1.0f + (ApproachRatio.Get() - 1.0f) * SmoothStep(progress)
                : 1.0f;
            const float radius = (std::max)(1.0f, OrbitRadius.Get()) * distanceScale;
            CameraPose pose{
                { std::sin(angle) * radius, Height.Get() * distanceScale,
                    -std::cos(angle) * radius },
                {} };

            if (shot == 1) {
                // 位置は固定し、異なる周期の小さな視線の揺れを重ねる。
                const float time = seconds * ShakeSpeed.Get();
                const float strength = ShakeStrength.Get();
                pose.aimOffset = {
                    strength * (std::sin(time * 1.7f) + 0.35f * std::sin(time * 4.3f)),
                    strength * (std::sin(time * 2.1f) + 0.3f * std::sin(time * 3.7f)),
                    0.0f };
            }
            return pose;
        }

        class ResultCameraFeature final : public ISceneFeature
        {
        public:
            const char* GetName() const override { return "ResultCamera"; }

            void PostSceneInitialize(SceneContext& ctx) override
            {
                elapsed_ = 0.0f;
                hasLooped_ = false;
                ApplyCamera(ctx);
            }

            void Update(SceneContext& ctx, SceneUpdatePhase phase) override
            {
                if (phase != SceneUpdatePhase::FrameStart) {
                    return;
                }
                elapsed_ += (std::max)(0.0f, Time::UnscaledDeltaTime());
                ApplyCamera(ctx);
            }

        private:
            void ApplyCamera(SceneContext& ctx)
            {
                // エディタ視点に切り替えても、操作するのはゲーム用カメラだけ。
                auto* camera = ctx.cameraManager
                    ? ctx.cameraManager->GetCamera(ctx.cameraManager->GetGameCameraName())
                    : nullptr;
                if (!camera || !ctx.gameObjectManager) {
                    return;
                }

                TransformComponent* monkey = nullptr;
                for (const auto& object : ctx.gameObjectManager->GetAllObjects()) {
                    if (object && !object->IsMarkedForDestroy()
                        && object->GetName() == "Result_monkey") {
                        monkey = object->GetComponent<TransformComponent>();
                        break;
                    }
                }
                if (!monkey) {
                    return;
                }

                const std::array<float, 3> durations{
                    (std::max)(0.1f, OrbitDuration.Get()),
                    (std::max)(0.1f, FixedDuration.Get()),
                    (std::max)(0.1f, ApproachDuration.Get()) };
                const float cycleDuration = durations[0] + durations[1] + durations[2];
                if (elapsed_ >= cycleDuration) {
                    elapsed_ = std::fmod(elapsed_, cycleDuration);
                    hasLooped_ = true;
                }

                size_t shot = 0;
                float shotTime = elapsed_;
                while (shot + 1 < durations.size() && shotTime >= durations[shot]) {
                    shotTime -= durations[shot++];
                }

                CameraPose pose = EvaluatePose(shot, shotTime / durations[shot], shotTime);
                const float blendTime = std::clamp(BlendDuration.Get(), 0.0f, durations[shot]);
                if ((shot != 0 || hasLooped_) && blendTime > 0.0f && shotTime < blendTime) {
                    const size_t previousShot = (shot + durations.size() - 1) % durations.size();
                    const CameraPose previous = EvaluatePose(previousShot, 1.0f, durations[previousShot]);
                    const float weight = SmoothStep(shotTime / blendTime);
                    pose.offset = previous.offset + (pose.offset - previous.offset) * weight;
                    pose.aimOffset = previous.aimOffset + (pose.aimOffset - previous.aimOffset) * weight;
                }

                // Result_monkey は親を持たない。初期化直後も行列転送を待たずに注視できる。
                const Vector3 target = monkey->Translate() + TargetOffset.Get();
                camera->SetTranslate(target + pose.offset);
                camera->LookAt(target + pose.aimOffset);
                camera->UpdateMatrix();
            }

            float elapsed_ = 0.0f;
            bool hasLooped_ = false;
        };
    }

    std::unique_ptr<CoreEngine::ISceneFeature> CreateResultCameraFeature()
    {
        return std::make_unique<ResultCameraFeature>();
    }
}

#include "pch.h"
#include "CameraSequenceFeature.h"

#include "Camera/Camera.h"
#include "Camera/CameraManager.h"
#include "Camera/Sequence/CameraSequence.h"
#include "Camera/Sequence/CameraSequenceEvaluator.h"
#include "Utility/FrameRate/Time.h"

namespace CoreEngine
{
    void CameraSequenceFeature::Initialize(SceneContext&)
    {
        CameraSequence::SetActivePlayer(&player_);
    }

    void CameraSequenceFeature::Update(SceneContext& ctx, SceneUpdatePhase phase)
    {
        if (phase != SceneUpdatePhase::PostLogic) {
            return;
        }

        // カメラが無くても時間は進める（シーン切り替え中に再生が凍りつかないように）
        player_.Update(Time::DeltaTime(), Time::UnscaledDeltaTime());

        if (!player_.IsActive()) {
            hasBlendFrom_ = false;
            observedPlayId_ = 0;
            return;
        }

        // 書き込むのは常にゲーム視点カメラ。ctx.gameViewCamera3D は「今覗いているカメラ」
        // なので、エディタ視点に切り替えている間はエディタカメラを指す。そちらは奪わない。
        Camera* camera = ctx.cameraManager
            ? ctx.cameraManager->GetCamera(ctx.cameraManager->GetGameCameraName())
            : nullptr;

        if (!camera) {
            return;
        }

        // 新しい再生が始まったフレームで、ゲーム側が決めた構図を繋ぎ元として控える。
        // この時点のカメラはすでに追従コンポーネント（OnLateUpdate）が書き終えている。
        if (player_.GetPlayId() != observedPlayId_) {
            observedPlayId_ = player_.GetPlayId();
            blendFrom_ = camera->CaptureSnapshot("SequenceBlendFrom");
            hasBlendFrom_ = true;
        }

        CameraSnapshot pose{};
        if (!player_.Evaluate(pose)) {
            return;
        }

        const float blendWeight = player_.GetBlendInWeight();
        if (hasBlendFrom_ && blendWeight < 1.0f) {
            pose = CameraSequenceEvaluator::Interpolate(
                blendFrom_, pose, blendWeight, EasingUtil::Type::EaseInOutCubic);
        }

        camera->RestoreSnapshot(pose);

        // CameraManager の通常更新（FrameStart）より後に姿勢を変えるため、
        // 描画前にここで行列と GPU 定数バッファを作り直す。
        camera->UpdateMatrix();
    }

    void CameraSequenceFeature::PostSceneFinalize(SceneContext&)
    {
        player_.Stop();
        hasBlendFrom_ = false;
        observedPlayId_ = 0;

        if (CameraSequence::GetActivePlayer() == &player_) {
            CameraSequence::SetActivePlayer(nullptr);
        }
    }
}

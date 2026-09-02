#include "pch.h"
#include "FogPass.h"

#include "Camera/View/ViewInfo.h"
#include "Graphics/Fog/FogManager.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/Render/FrameBlackboard.h"
#include "Graphics/Render/RenderGraph.h"
#include "Graphics/Render/RenderTarget/OffscreenRenderTarget.h"
#include "Graphics/Render/RenderTarget/RenderTargetManager.h"

namespace CoreEngine
{
    void FogPass::DeclareResources(RenderGraphBuilder& builder, [[maybe_unused]] const RenderContext& context)
    {
        // SceneDepth を読み、SceneColor へフォグを in-place 合成する（合成 CS が UAV で読み書きする）
        builder.Read(FrameBlackboard::SceneDepth, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        builder.Write(FrameBlackboard::SceneColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void FogPass::Execute(const RenderContext& context)
    {
        // GameView のみで有効（水面反射などの補助 View には適用しない）
        if (context.viewSettings.viewType != RenderViewType::GameView) {
            return;
        }

        if (!context.dxCommon || !context.fogManager || !context.renderTargetManager) {
            return;
        }

        // フォグを使うシーン（Update() を呼ぶシーン）で、かつ有効なときだけ合成する
        if (!context.fogManager->IsFogActive()) {
            return;
        }

        ID3D12GraphicsCommandList* cmdList = context.cmdList;
        if (!cmdList) {
            return;
        }

        // 深度復元にはフレーム先頭で確定したビューを使う（カメラを直接読むと
        // 読み取り時刻で行列が変わり、パスごとに違う復元結果になる）
        if (!context.frameViews) {
            return;
        }
        const ViewInfo& view = context.frameViews->GameView();
        if (!view.isValid) {
            return;
        }

        auto* sceneColorTarget = dynamic_cast<OffscreenRenderTarget*>(
            context.renderTargetManager->GetRenderTarget(context.viewSettings.sceneColorTargetName));
        if (!sceneColorTarget || !sceneColorTarget->GetResource()) {
            return;
        }

        // 深度は DeclareResources で Read 宣言した Blackboard の SceneDepth から取る
        D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthSrv{};
        if (!context.frameBlackboard
            || !context.frameBlackboard->TryGetSrvHandle(FrameBlackboard::SceneDepth, sceneDepthSrv)) {
            return;
        }

        context.fogManager->ApplyFog(
            cmdList,
            sceneColorTarget->Resource(),
            sceneColorTarget->GetUAVHandle(),
            sceneDepthSrv,
            view);
    }
}

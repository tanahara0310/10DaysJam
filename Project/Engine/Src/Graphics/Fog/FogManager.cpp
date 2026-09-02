#include "pch.h"
#include "Graphics/Fog/FogManager.h"

#include "Camera/View/ViewInfo.h"
#include "Graphics/Fog/Settings/FogCVars.h"
#include "Graphics/Fog/Shader/FogBindings.h"
#include "Graphics/RHI/Barrier/BarrierBatch.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RHI/Resource/GpuResource.h"
#include "Graphics/RHI/Resource/UploadRing.h"
#include "Graphics/RootSignature/ShaderBinder.h"
#include "Graphics/Shader/ICustomShaderProvider.h"
#include "Graphics/Shader/ShaderCompiler.h"
#include "Graphics/Shader/ShaderReflectionBuilder.h"
#include "Graphics/Shader/ShaderReflectionData.h"
#include "Utility/Logger/Logger.h"

#include <exception>

namespace CoreEngine
{
    namespace
    {
        /// @brief 合成 CS のシェーダー供給（パスはファイル名で AssetDatabase が解決する）
        class HeightFogShaderProvider final : public ICustomShaderProvider {
        public:
            std::wstring GetComputeShaderPath() const override { return L"HeightFog.CS.hlsl"; }
        };

        constexpr uint32_t kThreadGroupSize = 8;
    }

    bool FogManager::Initialize(GraphicsCore* graphicsCore)
    {
        graphicsCore_ = graphicsCore;
        if (!graphicsCore_) {
            return false;
        }

        ID3D12Device* device = graphicsCore_->GetDevice();
        if (!device) {
            return false;
        }

        // DXC は構築 1 回につき 1 個で足りる
        ShaderCompiler shaderCompiler;
        shaderCompiler.Initialize();

        ShaderReflectionBuilder reflectionBuilder;
        reflectionBuilder.Initialize(shaderCompiler.GetDxcUtils());

        const HeightFogShaderProvider provider;
        if (!pipeline_.Build(device, shaderCompiler, reflectionBuilder, provider)
            || !pipeline_.HasComputePSO()) {
            Logger::GetInstance().Warnf(LogCategory::Graphics,
                "FogManager: HeightFog コンピュートパイプラインの構築に失敗");
            return false;
        }

        const ShaderReflectionData* reflection = pipeline_.GetComputeReflection();
        if (!reflection) {
            Logger::GetInstance().Warnf(LogCategory::Graphics,
                "FogManager: HeightFog のリフレクションを取得できません");
            return false;
        }

        try {
            bindings_ = BindingTable::Resolve(*reflection, FogApplyBind::kDecls, "HeightFog");
        }
        catch (const std::exception&) {
            // 違反の内訳は BindingTable::Resolve が error ログへ出している
            return false;
        }

        pipelineReady_ = true;
        return true;
    }

    void FogManager::Update()
    {
        // Update() を呼ぶのはフォグを使うシーンのみ。このフレームは合成を有効にする
        fogActive_ = true;

        // 設定は CVar が保持する。ダーティ判定を持つ資源が無いので毎フレーム取り込んでよい
        FogCVars::LoadInto(settings_);
    }

    FogManager::FogConstants FogManager::BuildConstants(const ViewInfo& view) const
    {
        FogConstants constants{};
        constants.invViewProj = view.invViewProjection;
        constants.cameraWorldPos = view.position;
        constants.density = settings_.density;
        constants.fogColor = settings_.color;
        constants.heightFalloff = settings_.heightFalloff;
        constants.heightRef = settings_.heightRefM;
        constants.startDistance = settings_.startDistanceM;
        constants.maxOpacity = settings_.maxOpacity;
        // 0 以下は「ファークリップまで」の意味にする（設定を触らずに済ませるための既定動作）
        constants.skyDistance = (settings_.skyDistanceM > 0.0f)
            ? settings_.skyDistanceM : view.farZ;
        constants.applyToSky = settings_.applyToSky ? 1u : 0u;
        return constants;
    }

    void FogManager::ApplyFog(
        ID3D12GraphicsCommandList* cmdList,
        GpuResource& sceneColor,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorUav,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthSrv,
        const ViewInfo& view)
    {
        if (!cmdList || !graphicsCore_ || !pipelineReady_ || !view.isValid) {
            return;
        }

        // 定数は毎フレーム UploadRing から取る（専用バッファを 1 本持って上書きすると、
        // GPU が前フレームのディスパッチを実行する前に CPU が書き潰す）
        constantsAddress_ = graphicsCore_->GetUploadRing().AllocateConstants(BuildConstants(view));

        Barrier::Transition(cmdList, sceneColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        {
            namespace B = FogApplyBind;
            cmdList->SetPipelineState(pipeline_.GetComputePSO());
            cmdList->SetComputeRootSignature(pipeline_.GetComputeRootSignature());

            ShaderBinder binder(cmdList, ShaderBinder::Pipeline::Compute);
            binder.Set(bindings_[B::gFog], constantsAddress_);
            binder.Set(bindings_[B::gSceneDepth], sceneDepthSrv);
            binder.Set(bindings_[B::gOutput], sceneColorUav);
            binder.ValidateBeforeDraw(bindings_);
        }

        const D3D12_RESOURCE_DESC desc = sceneColor.Desc();
        cmdList->Dispatch(
            (static_cast<UINT>(desc.Width) + kThreadGroupSize - 1) / kThreadGroupSize,
            (desc.Height + kThreadGroupSize - 1) / kThreadGroupSize,
            1);

        // 書き込み完了を待たせてから、後続パス（Transparent 等）の想定状態へ戻す
        Barrier::UAV(cmdList, sceneColor);
        Barrier::Transition(cmdList, sceneColor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

#pragma once

#include "Graphics/Fog/Settings/FogSettings.h"
#include "Graphics/Pipeline/CustomShaderPipeline.h"
#include "Graphics/Shader/CBufferLayout.h"
#include "Graphics/Shader/CBufferReflectionCheck.h"
#include "Graphics/Shader/ShaderBindingContract.h"
#include "Math/MathCore.h"

#include <d3d12.h>

namespace CoreEngine
{
    class GpuResource;
    class GraphicsCore;
    struct ViewInfo;

    /// @brief 高さフォグシステムの窓口
    /// @details 設定・パイプラインを所有し、フレーム状態を定数バッファへ詰めて
    ///          SceneColor へ合成する。数式の実体はシェーダー側
    ///          （Assets/Shaders/Include/Common/Fog.hlsli）にあり、前方描画も同じものを使う。
    /// @note 大気散乱（AerialPerspective）とは効く距離帯が 2 桁以上違う別の媒質なので、
    ///       両方同時に有効でよい。重ねた結果は放射伝達として正しい
    ///       （カメラに近い媒質を後に合成する＝本パスが AerialPerspective より後）。
    class FogManager {
    public:
        /// @brief フォグ合成 CS が使う定数バッファ
        /// @note Assets/Shaders/Include/Common/Fog.hlsli の struct FogParameters と 1 対 1。
        ///       行列は転置せずそのまま入れる（HLSL 側は行ベクトル規約 mul(v, M)）
        struct FogConstants {
            Matrix4x4 invViewProj;      ///< View*Projection の逆行列
            Vector3   cameraWorldPos;   ///< カメラのワールド座標 [m]
            float     density;          ///< heightRef における消散係数 [1/m]
            Vector3   fogColor;         ///< フォグ色（リニア HDR）
            float     heightFalloff;    ///< 高さ方向の減衰率 [1/m]
            float     heightRef;        ///< density を与える基準高度 [m]
            float     startDistance;    ///< フォグが効き始める距離 [m]
            float     maxOpacity;       ///< 最大濃度 [0,1]
            float     skyDistance;      ///< 背景ピクセルのレイ長 [m]
            uint32_t  applyToSky;       ///< 背景にもフォグを掛けるか（HLSL の bool は 4 バイト）
            float     fogPad[3];
        };

        static constexpr Cb::Field kFogConstantsFields[] = {
            CB_FIELD(FogConstants, invViewProj),   CB_FIELD(FogConstants, cameraWorldPos),
            CB_FIELD(FogConstants, density),       CB_FIELD(FogConstants, fogColor),
            CB_FIELD(FogConstants, heightFalloff), CB_FIELD(FogConstants, heightRef),
            CB_FIELD(FogConstants, startDistance), CB_FIELD(FogConstants, maxOpacity),
            CB_FIELD(FogConstants, skyDistance),   CB_FIELD(FogConstants, applyToSky),
            CB_FIELD(FogConstants, fogPad),
        };
        CB_VERIFY_LAYOUT(FogConstants, kFogConstantsFields);
        CB_BIND_HLSL(FogConstants, kFogConstantsFields, "gFog");

    public:
        /// @brief 初期化（合成 CS のコンパイルと PSO 構築）
        /// @param graphicsCore デバイスと UploadRing の取得元
        /// @return 構築に成功したら true（失敗しても IsFogActive() が false になるだけで描画は続く）
        bool Initialize(GraphicsCore* graphicsCore);

        /// @brief フレーム更新（EnvironmentFeature::UpdateFog から毎フレーム呼ばれる）
        /// @details fogActive_ を立て、CVar の現在値を設定へ取り込む。
        ///          Update() を呼ばないシーンではフォグ合成そのものが走らない。
        void Update();

        /// @brief フレーム終端の後始末（RenderDomainContext が全 View 描画後に呼ぶ）
        void EndFrame() { fogActive_ = false; }

        /// @brief このフレームでフォグが要求されているか（Update() が呼ばれ、かつ有効かつ構築済み）
        bool IsFogActive() const { return fogActive_ && settings_.enabled && pipelineReady_; }

        /// @brief 合成パイプラインの構築に成功しているか
        /// @details false のときは r.Fog.Enabled を立ててもフォグは出ない（シェーダーのコンパイル失敗）。
        ///          エディタが原因を切り分けるために公開している
        bool IsPipelineReady() const { return pipelineReady_; }

        /// @brief 現在の設定（実体は FogCVars。Update が毎フレーム取り込む）
        /// @note 変更するときは FogCVars 側を Set すること（ここへ書いても次の Update で戻る）
        const FogSettings& GetSettings() const { return settings_; }

        /// @brief SceneColor へフォグを in-place 合成する
        /// @param cmdList         記録先コマンドリスト
        /// @param sceneColor      SceneColor（ステート追跡込み）
        /// @param sceneColorUav   SceneColor の UAV ハンドル
        /// @param sceneDepthSrv   SceneDepth の SRV ハンドル
        /// @param view            描画に使われたビュー（深度復元の行列はここから取る）
        void ApplyFog(
            ID3D12GraphicsCommandList* cmdList,
            GpuResource& sceneColor,
            D3D12_GPU_DESCRIPTOR_HANDLE sceneColorUav,
            D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthSrv,
            const ViewInfo& view);

        /// @brief 今フレームのフォグ定数バッファの GPU 仮想アドレス
        /// @details 半透明・水面・パーティクルが Fog.hlsli の ApplyFog を呼ぶときに差す。
        ///          全画面合成と同じ実体を読むので数式がずれない。
        /// @warning 今フレームの ApplyFog() が走った後のみ有効（0 = 未確保）
        D3D12_GPU_VIRTUAL_ADDRESS GetConstantsGpuAddress() const { return constantsAddress_; }

    private:
        /// @brief 設定とビューから今フレームの定数バッファ内容を作る
        FogConstants BuildConstants(const ViewInfo& view) const;

        GraphicsCore* graphicsCore_ = nullptr;

        CustomShaderPipeline pipeline_{};
        BindingTable bindings_{};
        bool pipelineReady_ = false;

        FogSettings settings_{};

        /// @brief このフレームで Update() が呼ばれフォグが要求されたか
        bool fogActive_ = false;

        /// @brief 今フレームぶんのフォグ定数（UploadRing 上）。フレームを跨いで持ち越さない
        D3D12_GPU_VIRTUAL_ADDRESS constantsAddress_ = 0;
    };
}

#pragma once

#include "../PostEffectComputeBase.h"
#include "../ILoadingScreenEffect.h"
#include <wrl.h>
#include <d3d12.h>

namespace CoreEngine
{
    /// @brief トロッコが走るローディング画面（CS方式）
    /// @details 暗転した画面の下部で、猿を乗せたトロッコがレールの上を走り続ける。
    ///          読み込みの進捗は「右から近づいてくる駅までの距離」で表す。
    ///          進捗が止まってもトロッコは走り続けるので画面が固まって見えない
    ///          ―― シーン構築が 1 フレーム 1 ステップで、所要時間が読めないため。
    /// @note 絵は Assets/Textures/loading_*.png（.obj から焼いたスプライト）。
    ///       車輪だけは trolley.obj に存在しないのでシェーダーが手続き的に描く。
    ///       レイアウト値は CVar（"r.TrolleyLoading.*"）が保持する。
    class TrolleyLoading : public PostEffectComputeBase, public ILoadingScreenEffect {
    public:
        /// @brief パラメータ構造体（GPU 定数バッファのレイアウト）
        /// @note 値の意味は TrolleyLoading.CS.hlsl 側と 1 対 1。位置と大きさは
        ///       すべて「縦 1080 基準のピクセル」で、実解像度へはシェーダーが拡大する
        struct TrolleyParams {
            float screenAlpha = 0.0f;   // 表示強度（実行時値）
            float time        = 0.0f;   // 経過時間（実行時値）
            float progress    = 0.0f;   // 読み込みの進捗（実行時値）
            float speed       = 336.0f; // レールが流れる速さ（px/秒）

            float parallax    = 0.32f;  // 奥の景色の速度比
            float railY       = 0.87f;  // レール上端（画面高さに対する比率）
            float cartX       = 0.30f;  // トロッコ左端（画面幅に対する比率）
            float bobAmp      = 4.0f;   // 上下の揺れ幅

            float tiltDegrees = 1.6f;   // 前後の傾き
            float wheelRadius = 34.0f;  // 車輪の半径
            float wheelInset  = 52.0f;  // 車体の端から車輪中心までの距離
            float wheelDrop   = 16.0f;  // レール上端から車輪中心までの距離

            float cartLift    = 28.0f;  // レール上端から車体下端までの距離
            float stationGoal = 340.0f; // 進捗 1.0 で駅が来る位置
            float stationDrop = 8.0f;   // レール上端から駅の下端までの距離
            float sceneryDrop = 4.0f;   // レール上端から景色の下端までの距離

            float scale       = 0.72f;  // 全体の拡大率（上の距離とスプライトへ一括で掛かる）
            // 定数バッファは 16 バイト単位なので、末尾の 1 行を詰め物で埋める。
            // 省くと Cb::Verify が「全体サイズ不一致」で弾く
            float scalePad0   = 0.0f;
            float scalePad1   = 0.0f;
            float scalePad2   = 0.0f;
        };

        static constexpr Cb::Field kTrolleyParamsFields[] = {
            CB_FIELD(TrolleyParams, screenAlpha), CB_FIELD(TrolleyParams, time),
            CB_FIELD(TrolleyParams, progress),    CB_FIELD(TrolleyParams, speed),
            CB_FIELD(TrolleyParams, parallax),    CB_FIELD(TrolleyParams, railY),
            CB_FIELD(TrolleyParams, cartX),       CB_FIELD(TrolleyParams, bobAmp),
            CB_FIELD(TrolleyParams, tiltDegrees), CB_FIELD(TrolleyParams, wheelRadius),
            CB_FIELD(TrolleyParams, wheelInset),  CB_FIELD(TrolleyParams, wheelDrop),
            CB_FIELD(TrolleyParams, cartLift),    CB_FIELD(TrolleyParams, stationGoal),
            CB_FIELD(TrolleyParams, stationDrop), CB_FIELD(TrolleyParams, sceneryDrop),
            CB_FIELD(TrolleyParams, scale),       CB_FIELD(TrolleyParams, scalePad0),
            CB_FIELD(TrolleyParams, scalePad1),   CB_FIELD(TrolleyParams, scalePad2),
        };
        CB_VERIFY_LAYOUT(TrolleyParams, kTrolleyParamsFields);
        CB_BIND_HLSL(TrolleyParams, kTrolleyParamsFields, "TrolleyParams");

    public:
        TrolleyLoading() = default;
        ~TrolleyLoading() = default;

        /// @brief CSエフェクト実行
        void Dispatch(
            D3D12_GPU_DESCRIPTOR_HANDLE inputSrvHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE outputUavHandle,
            uint32_t width,
            uint32_t height) override;

        /// @brief 更新処理（経過時間の積算）
        void PrepareFrame(const PostEffectFrameContext& ctx) override;

        /// @brief ImGuiでパラメータを調整
        void DrawImGui() override;

        // ---- ILoadingScreenEffect ----
        void SetScreenAlpha(float alpha) override;
        void SetProgress(float progress) override;

        /// @brief 進捗ゲージの表示強度（この画面では使わない）
        /// @details 進捗は「駅までの距離」で常時見えているので、別建てのゲージを
        ///          出す必要が無い。呼ばれても何もしないのが正しい挙動
        void SetGaugeAlpha(float alpha) override;

        void SetLoadingEnabled(bool enabled) override { SetEnabled(enabled); }

    protected:
        /// @brief 有効/無効は CVar "r.<Effect>.Enabled" が保持する
        CVar<bool>* GetEnabledCVar() const override;

        std::string  GetEffectName()        const override { return "TrolleyLoading"; }
        std::wstring GetComputeShaderPath() const override { return L"TrolleyLoading.CS.hlsl"; }

        /// @brief 定数バッファ生成とスプライトの読み込み
        void OnCreateConstantBuffers() override;

    private:
        void UpdateConstantBuffer();

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> trolleyParamsCB_;
        TrolleyParams* mappedTrolleyParams_ = nullptr;

        D3D12_GPU_DESCRIPTOR_HANDLE cartHandle_    = {};
        D3D12_GPU_DESCRIPTOR_HANDLE railHandle_    = {};
        D3D12_GPU_DESCRIPTOR_HANDLE stationHandle_ = {};
        D3D12_GPU_DESCRIPTOR_HANDLE sceneryHandle_ = {};

        // 実行時状態（保存対象ではない）
        float screenAlpha_     = 0.0f;
        float timeAccumulator_ = 0.0f;
        float progress_        = 0.0f;
    };
}

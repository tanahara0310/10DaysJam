#include "pch.h"
#include "SkyFogFeature.h"

#include "Graphics/Fog/Settings/FogCVars.h"
#include "Math/Vector/Vector4.h"
#include "Scene/Feature/ISceneFeature.h"
#include "Utility/CVar/CVar.h"

#include <memory>

using namespace CoreEngine;

namespace {

    // ──────────────────────────────────────────────────────────
    // 雲（フォグ）の調整値
    // ──────────────────────────────────────────────────────────
    // 狙いは「ステージのブロックより下だけを雲で埋める」。ステージ自体には一切
    // 掛けないので、遠くのブロックも手前と同じようにはっきり見える。
    //
    // ■ 高さで切る（BaseHeight / HeightFalloff）
    //   密度モデルは rho(y) = Density * exp(-HeightFalloff * (y - BaseHeight))。
    //   BaseHeight が雲の上面で、そこから上は HeightFalloff の速さで薄くなる。
    //   上面を地面ブロックの底（y = -0.5m）に置いてあるので、ステージ上のものは
    //   すべて雲より上になる。ブロックの天面（地面 0.46m / 水 0.15m）の減光は
    //   1〜3% で、実質そのまま見える。逆にブロックの下へ抜ける視線と、
    //   ステージの外（何も無い空間）は雲で埋まる。
    //
    // ■ 何も無い空間を塗る（ApplyToSky）
    //   ステージの外は描画物が無い＝背景ピクセルなので、ここへ掛けないと
    //   雲がステージの真下にしか出ない。エンジン既定どおり有効にしておく。
    //
    // ■ この設定は「既定床が無い」前提
    //   r.Ground.Enable = false（無限床を切ってある）のが前提。床を戻すと
    //   y = 0 の板が雲の上面より上に出るので、雲がその板に隠れて見えなくなる。
    //
    // ■ HeightFalloff を下げすぎない／上げすぎない
    //   下げる（0.2 以下）と雲が上へ広がってステージまで霞む。上げる（10 以上）と
    //   境目が刃物のように鋭くなり、見下ろしカメラでは「白い床」に見える。4 前後。
    //
    // 値は CVars.json へ自動保存され、インスペクターの「ゲーム設定」から編集できる。

    CVar<bool> cvEnabled{
        "Game.Fog.Enabled", true,
        "ゲームシーンで雲を出す。切るとシーン開始前のフォグ設定へ戻る" };

    CVar<float> cvBaseHeight{
        "Game.Fog.BaseHeight", -0.5f,
        "雲の上面の高さ [m]。ここより上は雲が掛からない。"
        "地面ブロックの底（-0.5m）に合わせてある。上げるほどステージが雲へ沈む",
        CVarRange{ -10.0f, 20.0f } };

    CVar<float> cvHeightFalloff{
        "Game.Fog.HeightFalloff", 4.0f,
        "上面から上へどれだけ速く薄くなるか [1/m]。大きいほど境目がくっきりする。"
        "0.2 以下にするとステージまで霞み、10 以上にすると境目が「白い床」に見える",
        CVarRange{ 0.0f, 20.0f } };

    CVar<float> cvDensity{
        "Game.Fog.Density", 1.0f,
        "上面の高さでの雲の濃さ [1/m]。上げるとブロックの側面が下から雲へ埋もれていく",
        CVarRange{ 0.0f, 2.0f } };

    CVar<float> cvMaxOpacity{
        "Game.Fog.MaxOpacity", 1.0f,
        "雲の濃さの上限。1 未満にすると、いちばん濃いところでも下の色が透ける",
        CVarRange{ 0.0f, 1.0f } };

    CVar<float> cvStartDistance{
        "Game.Fog.StartDistance", 0.0f,
        "雲が効き始めるカメラからの距離 [m]。高さで切っているので通常は 0 でよい",
        CVarRange{ 0.0f, 200.0f } };

    CVar<Vector4> cvColor{
        "Game.Fog.Color", Vector4{ 0.85f, 0.90f, 0.98f, 1.0f },
        "雲の色（色味のみ。明るさは Brightness が持つ）" };

    CVar<float> cvBrightness{
        "Game.Fog.Brightness", 1.2f,
        "雲の明るさ倍率。シーンより明るくすると白飛びして見えるので、"
        "1 前後から少しずつ上げること",
        CVarRange{ 0.0f, 20.0f } };

    CVar<float> cvSkyColorBlend{
        "Game.Fog.SkyColorBlend", 0.0f,
        "雲の色を空の色（大気散乱の輝度）へ寄せる量。0 なら Color × Brightness が"
        "そのまま出るので明るさを自分で決められる。上げると空へ自動で馴染むが、"
        "空の輝度は数十のオーダーなので一気に明るくなる",
        CVarRange{ 0.0f, 1.0f } };

    CVar<Vector4> cvSunTint{
        "Game.Fog.SunTint", Vector4{ 1.0f, 0.95f, 0.86f, 1.0f },
        "太陽方向での雲の色味（雲の色への倍率）" };

    CVar<float> cvSunGain{
        "Game.Fog.SunGain", 1.6f,
        "太陽方向での雲の明るさ倍率。1 で内散乱なし",
        CVarRange{ 1.0f, 8.0f } };

    CVar<float> cvSunExponent{
        "Game.Fog.SunExponent", 8.0f,
        "太陽まわりの光り方の鋭さ。大きいほど太陽の周りだけが狭く光る",
        CVarRange{ 1.0f, 128.0f } };

    // ──────────────────────────────────────────────────────────
    // エンジン側フォグ（r.Fog.*）の読み書き
    // ──────────────────────────────────────────────────────────

    /// @brief このシーンが触る r.Fog.* 一式
    /// @details 退避と書き戻しを同じ形で行うためのまとめ。r.Fog.SkyDistance は
    ///          背景ピクセルのレイ長で、既定の 5000m のままで足りるので触らない。
    struct EngineFogState {
        bool    enabled;
        Vector4 color;
        float   colorIntensity;
        float   density;
        float   heightFalloff;
        float   heightRef;
        float   startDistance;
        float   maxOpacity;
        bool    applyToSky;
        float   skyColorBlend;
        Vector4 sunTint;
        float   sunGain;
        float   sunExponent;
    };

    /// @brief r.Fog.* の現在値を読み出す
    EngineFogState ReadEngineFog()
    {
        return {
            FogCVars::Enabled.Get(),
            FogCVars::Color.Get(),
            FogCVars::ColorIntensity.Get(),
            FogCVars::Density.Get(),
            FogCVars::HeightFalloff.Get(),
            FogCVars::HeightRefM.Get(),
            FogCVars::StartDistanceM.Get(),
            FogCVars::MaxOpacity.Get(),
            FogCVars::ApplyToSky.Get(),
            FogCVars::SkyColorBlend.Get(),
            FogCVars::SunTint.Get(),
            FogCVars::SunScatteringGain.Get(),
            FogCVars::SunScatteringExponent.Get(),
        };
    }

    /// @brief r.Fog.* へ書き戻す
    /// @note CVar::Set は値が実際に変わったときだけ通知するので、毎フレーム呼んでよい
    void WriteEngineFog(const EngineFogState& state)
    {
        FogCVars::Enabled.Set(state.enabled);
        FogCVars::Color.Set(state.color);
        FogCVars::ColorIntensity.Set(state.colorIntensity);
        FogCVars::Density.Set(state.density);
        FogCVars::HeightFalloff.Set(state.heightFalloff);
        FogCVars::HeightRefM.Set(state.heightRef);
        FogCVars::StartDistanceM.Set(state.startDistance);
        FogCVars::MaxOpacity.Set(state.maxOpacity);
        FogCVars::ApplyToSky.Set(state.applyToSky);
        FogCVars::SkyColorBlend.Set(state.skyColorBlend);
        FogCVars::SunTint.Set(state.sunTint);
        FogCVars::SunScatteringGain.Set(state.sunGain);
        FogCVars::SunScatteringExponent.Set(state.sunExponent);
    }

    /// @brief 調整値から、このシーンで使う r.Fog.* を組み立てる
    EngineFogState BuildGameFog()
    {
        EngineFogState state{};
        state.enabled = true;
        state.color = cvColor.Get();
        state.colorIntensity = cvBrightness.Get();
        state.density = cvDensity.Get();
        state.heightFalloff = cvHeightFalloff.Get();
        state.heightRef = cvBaseHeight.Get();
        state.startDistance = cvStartDistance.Get();
        state.maxOpacity = cvMaxOpacity.Get();
        // ステージの外（描画物が無いピクセル）にも掛ける。ここが雲の本体で、
        // 切るとステージの真下だけしか雲にならない
        state.applyToSky = true;
        state.skyColorBlend = cvSkyColorBlend.Get();
        state.sunTint = cvSunTint.Get();
        state.sunGain = cvSunGain.Get();
        state.sunExponent = cvSunExponent.Get();
        return state;
    }

    // ──────────────────────────────────────────────────────────
    // Feature
    // ──────────────────────────────────────────────────────────

    /// @brief ゲームシーンの間だけ、ステージより下を雲で埋める Feature
    /// @details フォグ設定はエンジン寿命の CVar（r.Fog.*）なので、シーン開始時に現在値を
    ///          退避し、終了時に書き戻す。タイトル・リザルトへ持ち出さないため。
    /// @note シーン中は毎フレーム Game.Fog.* を r.Fog.* へ流し込む。エディタの
    ///       「Height Fog」から r.Fog.* を直接いじっても次のフレームで戻るので、
    ///       このシーンの見た目は「ゲーム設定」の Game.Fog.* だけで決まる。
    class SkyFogFeature final : public ISceneFeature {
    public:
        const char* GetName() const override { return "GameSkyFog"; }

        void Initialize(SceneContext&) override
        {
            savedFog_ = ReadEngineFog();
            Sync();
        }

        void Update(SceneContext&, SceneUpdatePhase phase) override
        {
            // フォグ設定を読むのは EnvironmentFeature（PostLogic）なので、
            // それより前のフェーズで流し込む
            if (phase == SceneUpdatePhase::FrameStart) {
                Sync();
            }
        }

        /// @brief 停止中も回す（止めると「ゲーム設定」で値を変えても画面が変わらない）
        bool RunsWhileStopped() const override { return true; }

        void Finalize(SceneContext&) override { WriteEngineFog(savedFog_); }

    private:
        /// @brief 調整値を r.Fog.* へ反映する（無効なら退避した値へ戻す）
        void Sync() const
        {
            WriteEngineFog(cvEnabled.Get() ? BuildGameFog() : savedFog_);
        }

        /// シーン開始時点の r.Fog.*（シーン終了時にここへ戻す）
        EngineFogState savedFog_{};
    };
}

std::unique_ptr<CoreEngine::ISceneFeature> GameComponents::CreateSkyFogFeature()
{
    return std::make_unique<SkyFogFeature>();
}

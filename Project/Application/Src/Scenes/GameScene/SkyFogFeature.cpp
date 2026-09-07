#include "pch.h"
#include "SkyFogFeature.h"

#include "EngineSystem/EngineSystem.h"
#include "Graphics/Fog/Settings/FogCVars.h"
#include "Graphics/Light/Light.h"
#include "Graphics/Light/LightManager.h"
#include "Math/MathCore.h"
#include "Math/Vector/Vector4.h"
#include "Scene/Feature/ISceneFeature.h"
#include "Utility/CVar/CVar.h"

#include <algorithm>
#include <cmath>
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
    //   すべて雲より上になる。ブロックの天面（地面 0.5m / 水 0.15m）の減光は
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
    // ■ 夜は雲も暗くする（NightBrightnessEV）
    //   フォグ色は Color × Brightness の絶対値で、FogManager がそのまま定数バッファへ
    //   入れる（時刻には追従しない）。一方でサーフェスは月光 80lx ＝ 太陽 100000lx の
    //   1/1250 まで落ち、自動露出は TimeOfDayFeature が上限 +7.5EV（≒×180）へ張り付か
    //   せる。昼と同じ色のままだと雲だけが露出に持ち上げられて白飛びし、ステージへ
    //   掛かるわずか 1〜3% の雲まで画面上では真っ白になる（＝ステージが霞んで見える）。
    //   そこで太陽高度から「夜の度合い」を出し、Brightness を EV で落として釣り合わせる。
    //
    // 値は CVars.json へ自動保存され、インスペクターの「ゲーム設定」から編集できる。

    CVar<bool> cvEnabled{
        "Game.Fog.Enabled", true,
        "ゲーム・リザルトシーンで雲を出す。切るとシーン開始前のフォグ設定へ戻る" };

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

    CVar<float> cvNightBrightnessEV{
        "Game.Fog.NightBrightnessEV", -10.0f,
        "夜に Brightness を何段（EV）落とすか。0 にすると昼と同じ色のままになり、"
        "夜の自動露出に持ち上げられて雲が白飛びする。"
        "既定 -10（≒1/1000）は月光 80lx と太陽 100000lx の比に合わせた値",
        CVarRange{ -16.0f, 0.0f } };

    CVar<float> cvNightStartElevationDeg{
        "Game.Fog.NightStartElevationDeg", 5.0f,
        "暗くし始める太陽高度 [deg]。Game.StageLights.OnElevationDeg と揃えてある",
        CVarRange{ -20.0f, 30.0f } };

    CVar<float> cvNightFullElevationDeg{
        "Game.Fog.NightFullElevationDeg", -6.0f,
        "落としきる太陽高度 [deg]（-6 = 市民薄明の終わり）。"
        "Game.StageLights.FullElevationDeg と揃えてある",
        CVarRange{ -30.0f, 20.0f } };

    // ──────────────────────────────────────────────────────────
    // 夜の度合い
    // ──────────────────────────────────────────────────────────

    /// @brief 太陽の高度角 [deg]（太陽が無いシーンは昼として扱う）
    /// @note TimeOfDayFeature は地平線下でも太陽ライトの向きを更新し続けるので、
    ///       夜は素直に負の値になる。月は別ライトなのでここには出てこない
    float ComputeSunElevationDeg(LightManager& lightManager)
    {
        const Light* sun = lightManager.GetAtmosphereSunLight();
        if (!sun) {
            return 90.0f;
        }
        // ライト方向は「太陽 → 地表」なので、太陽を見る方向の Y が sin(高度)
        const Vector3 direction = Normalize(sun->direction);
        return std::asin(std::clamp(-direction.y, -1.0f, 1.0f))
            * MathCore::Constants::kRadToDeg;
    }

    /// @brief 太陽高度から夜の度合い（0 = 昼 / 1 = 夜）を求める
    /// @note StageLightsFeature::ComputeLitRatio と同じ式。灯りと雲の変わり方を揃える
    float ComputeNightFactor(float sunElevationDeg)
    {
        const float start = cvNightStartElevationDeg.Get();
        const float full = cvNightFullElevationDeg.Get();

        float t = (start > full)
            ? std::clamp((start - sunElevationDeg) / (start - full), 0.0f, 1.0f)
            : ((sunElevationDeg <= start) ? 1.0f : 0.0f);

        // 変わり始めと変わり終わりの角を丸める（線形だと切り替わりが唐突に見える）
        return t * t * (3.0f - 2.0f * t);
    }

    /// @brief 夜の度合いから Brightness へ掛ける倍率を求める
    /// @details 補間は EV（対数）で行う。明るさは対数で効くので線形に混ぜると、
    ///          薄明のあいだ雲だけが明るいまま取り残される。昼（0）では 1 倍で恒等
    float ComputeNightBrightnessScale(float nightFactor)
    {
        return std::exp2(cvNightBrightnessEV.Get() * nightFactor);
    }

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
    /// @param nightBrightnessScale 夜の落とし込み倍率（1 = 昼。ComputeNightBrightnessScale）
    EngineFogState BuildGameFog(float nightBrightnessScale)
    {
        EngineFogState state{};
        state.enabled = true;
        state.color = cvColor.Get();
        // 色ではなく明るさ側を落とす。色を暗くすると Color の色味そのものが
        // 分からなくなり、インスペクターで昼の色を決められなくなる
        state.colorIntensity = cvBrightness.Get() * nightBrightnessScale;
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

    /// @brief 登録したシーン（ゲーム・リザルト）の間だけ、ステージより下を雲で埋める Feature
    /// @details フォグ設定はエンジン寿命の CVar（r.Fog.*）なので、シーン開始時に現在値を
    ///          退避し、終了時に書き戻す。タイトルへ持ち出さないため。
    /// @note シーン中は毎フレーム Game.Fog.* を r.Fog.* へ流し込む。エディタの
    ///       「Height Fog」から r.Fog.* を直接いじっても次のフレームで戻るので、
    ///       このシーンの見た目は「ゲーム設定」の Game.Fog.* だけで決まる。
    /// @note オン/オフは 2 系統ある。シーン側の SetEnabled()（開発者の指定）と
    ///       CVar Game.Fog.Enabled（「ゲーム設定」からの全体スイッチ）の AND。
    ///       どちらで切っても、書き戻す先はシーン開始時点の r.Fog.* で同じ。
    class SkyFogFeature final : public GameComponents::ISkyFogFeature {
    public:
        explicit SkyFogFeature(bool enabled) : sceneEnabled_(enabled) {}

        const char* GetName() const override { return "GameSkyFog"; }

        /// @brief シーン側のオン/オフ（CVar Game.Fog.Enabled との AND で決まる）
        void SetEnabled(bool enabled) override
        {
            if (sceneEnabled_ == enabled) {
                return;
            }
            sceneEnabled_ = enabled;
            // Initialize 前は savedFog_ が空なので書き戻してはいけない
            // （初期化時の Sync() がこの値を見て反映する）
            if (initialized_) {
                Sync();
            }
        }

        bool IsEnabled() const override { return sceneEnabled_; }

        void Initialize(SceneContext& ctx) override
        {
            savedFog_ = ReadEngineFog();
            initialized_ = true;
            RefreshNightBrightnessScale(ctx);
            Sync();
        }

        void Update(SceneContext& ctx, SceneUpdatePhase phase) override
        {
            // フォグ設定を読むのは EnvironmentFeature（PostLogic）なので、
            // それより前のフェーズで流し込む。
            // 太陽を動かす TimeOfDayFeature も同じ FrameStart だが、GameScene が
            // 先に登録しているので、ここで読む高度はこのフレームの値になる
            if (phase == SceneUpdatePhase::FrameStart) {
                RefreshNightBrightnessScale(ctx);
                Sync();
            }
        }

        /// @brief 停止中も回す（止めると「ゲーム設定」で値を変えても画面が変わらない）
        bool RunsWhileStopped() const override { return true; }

        void Finalize(SceneContext&) override { WriteEngineFog(savedFog_); }

    private:
        /// @brief 太陽高度から夜の落とし込み倍率を求め直す
        /// @note ライトが引けないフレームは直前の倍率を保つ。1 へ戻すと、
        ///       シーン遷移などで一瞬だけ夜に雲が白く光ることになる
        void RefreshNightBrightnessScale(SceneContext& ctx)
        {
            auto* lightManager = ctx.engine
                ? ctx.engine->GetService<LightManager>() : nullptr;
            if (!lightManager) {
                return;
            }
            nightBrightnessScale_ =
                ComputeNightBrightnessScale(
                    ComputeNightFactor(ComputeSunElevationDeg(*lightManager)));
        }

        /// @brief 調整値を r.Fog.* へ反映する（無効なら退避した値へ戻す）
        /// @note シーン側（sceneEnabled_）と「ゲーム設定」の CVar は AND。
        ///       どちらか一方でも切れば雲は出ない
        void Sync() const
        {
            const bool show = sceneEnabled_ && cvEnabled.Get();
            WriteEngineFog(show ? BuildGameFog(nightBrightnessScale_) : savedFog_);
        }

        /// シーン開始時点の r.Fog.*（シーン終了時にここへ戻す）
        EngineFogState savedFog_{};

        /// 直近の夜の落とし込み倍率（1 = 昼）
        float nightBrightnessScale_ = 1.0f;

        /// シーン側のオン/オフ（CVar Game.Fog.Enabled とは独立）
        bool sceneEnabled_ = true;

        /// Initialize 済みか（savedFog_ が有効かの判定に使う）
        bool initialized_ = false;
    };
}

std::unique_ptr<GameComponents::ISkyFogFeature>
GameComponents::CreateSkyFogFeature(bool enabled)
{
    return std::make_unique<SkyFogFeature>(enabled);
}

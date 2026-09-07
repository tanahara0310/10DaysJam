// TrolleyLoading.CS.hlsl - トロッコが走るローディング画面 コンピュートシェーダー
//
// 絵は Assets/Textures/loading_*.png（.obj から正射投影で焼いたボクセルのスプライト）を
// 奥から順に重ねるだけ。車輪だけは trolley.obj に存在しないのでここで手続き的に描く。
//
// ■ 座標
//   位置は「縦 1080 基準のピクセル」で持ち、uiScale で実解像度へ拡大する。
//   基準解像度を変えるとレイアウトが全部ずれるので kReferenceHeight は触らないこと。
//
// ■ 色空間
//   この段（PostTonemap）の出力はリニアで、sRGB へのエンコードは最終提示で掛かる。
//   一方 Load が返すのは PNG の生の値（sRGB）なので、スプライトも定数で書いた色
//   （車輪）も SrgbToLinear を通してから合成する。LoadingScreen.CS.hlsl と同じ扱い。
//   これを省くと枕木の (133,87,43) が画面上で (189,158,115) まで浮く。

#include "ShaderMath.hlsli" // PI / TWO_PI

Texture2D<float4> gTexture : register(t0); // 合成前の画面
Texture2D<float4> gCart    : register(t1); // トロッコ＋猿
Texture2D<float4> gRail    : register(t2); // レール 1 周期
Texture2D<float4> gStation : register(t3); // 駅（進捗の到達点）
Texture2D<float4> gScenery : register(t4); // 奥の景色（木・岩を焼き込んだ帯）
RWTexture2D<float4> gOutput : register(u0);

cbuffer TrolleyParams : register(b0)
{
    float screenAlpha;  // 表示強度 (0.0 = 非表示, 1.0 = 完全表示)
    float time;         // 経過時間（秒）
    float progress;     // 読み込みの進捗 (0.0〜1.0)
    float speed;        // レールが流れる速さ（1080 基準の px/秒）

    float parallax;     // 奥の景色の速度比
    float railY;        // レール上端（画面高さに対する比率）
    float cartX;        // トロッコ左端（画面幅に対する比率）
    float bobAmp;       // 上下の揺れ幅（1080 基準の px）

    float tiltDegrees;  // 前後の傾き（度）
    float wheelRadius;  // 車輪の半径（1080 基準の px）
    float wheelInset;   // 車体の端から車輪中心までの距離
    float wheelDrop;    // レール上端から車輪中心までの距離

    float cartLift;     // レール上端から車体下端までの距離
    float stationGoal;  // 進捗 1.0 で駅が来る位置（トロッコ左端からの距離）
    float stationDrop;  // レール上端から駅の下端までの距離
    float sceneryDrop;  // レール上端から景色の下端までの距離

    float scale;        // 全体の拡大率。上の距離もスプライトも一括で掛かる
    float scalePad0;    // 16 バイト境界を埋めるための詰め物（C++ 側と対で持つ）
    float scalePad1;
    float scalePad2;
};

cbuffer ScreenParams : register(b1)
{
    uint screenWidth;
    uint screenHeight;
    float2 pad;
};

static const uint  kGroupSize       = 8;
static const float kReferenceHeight = 1080.0f; // レイアウト値の基準解像度
static const float kEdgeWidth       = 1.5f;    // 車輪の輪郭のぼかし幅（ピクセル）
static const float kStationEnter    = 60.0f;   // 駅が画面右外から現れる距離

// 車輪の色（sRGB）。モデルに車輪が無いのでここで描く
static const float3 kTireColor  = float3(0.165f, 0.165f, 0.180f);
static const float3 kSpokeColor = float3(0.329f, 0.329f, 0.353f);
static const float3 kHubColor   = float3(0.408f, 0.408f, 0.431f);
static const float3 kInnerColor = float3(0.094f, 0.094f, 0.110f);

float3 SrgbToLinear(float3 c)
{
    float3 lo = c / 12.92f;
    float3 hi = pow(max(c + 0.055f, 0.0f) / 1.055f, 2.4f);
    return lerp(lo, hi, step(0.04045f, c));
}

float2 Rotate(float2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

/// スプライトを 1 テクセル読む。local はスプライト左上を原点としたテクセル座標
/// @return rgb はリニアへ変換済み、a はそのまま（アルファはガンマを持たない）
/// @note 点サンプルなのでボクセルのドットが潰れない（拡大してもエッジが甘くならない）
/// @note リニア化はここ 1 か所に閉じてある。呼び出し側で掛け忘れると色が浮く
float4 LoadSprite(Texture2D<float4> tex, float2 local)
{
    uint w, h;
    tex.GetDimensions(w, h);
    int2 p = int2(floor(local));
    if (p.x < 0 || p.y < 0 || p.x >= (int)w || p.y >= (int)h)
    {
        return (float4)0.0f;
    }
    float4 texel = tex.Load(int3(p, 0));
    return float4(SrgbToLinear(texel.rgb), texel.a);
}

/// 手続き的な車輪。タイヤ・スポーク・ハブの 3 層
/// @param p   車軸を原点とした画面ピクセル座標
/// @param ang 回転角（ラジアン）
float3 DrawWheel(float3 base, float2 p, float r, float ang, float alpha)
{
    float d = length(p);
    if (d > r + kEdgeWidth)
    {
        return base;
    }

    float a = atan2(p.y, p.x) - ang;

    float3 c;
    if (d > r * 0.78f)
    {
        c = kTireColor;
    }
    else if (d < r * 0.20f)
    {
        c = kHubColor;
    }
    else if (abs(cos(a * 2.0f)) > 0.95f || abs(sin(a * 2.0f)) > 0.95f)
    {
        c = kSpokeColor;
    }
    else
    {
        c = kInnerColor;
    }

    float cover = 1.0f - smoothstep(r - kEdgeWidth, r + kEdgeWidth, d);
    return lerp(base, SrgbToLinear(c), cover * alpha);
}

[numthreads(kGroupSize, kGroupSize, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= screenWidth || dispatchId.y >= screenHeight)
    {
        return;
    }

    float3 color = gTexture.Load(int3(dispatchId.xy, 0)).rgb;

    if (screenAlpha > 0.001f)
    {
        // 位置も大きさも 1080 基準の距離に uiScale を掛けて出しているので、
        // ここへ scale を畳み込むだけで全体が相似のまま拡大・縮小する。
        // 走る速さ・跳ねる周期・車輪の転がりは 1080 基準の距離のまま計算するため、
        // 縮めても「レール 1 本ぶん進むと 1 回跳ねる」の関係は崩れない
        float  uiScale = (float)screenHeight / kReferenceHeight * scale;
        float2 pix     = (float2)dispatchId.xy + 0.5f;
        float  railTop = railY * (float)screenHeight;
        float  dist    = speed * time; // 1080 基準で進んだ距離

        uint railW,  railH;  gRail.GetDimensions(railW, railH);
        uint cartW,  cartH;  gCart.GetDimensions(cartW, cartH);
        uint sceneW, sceneH; gScenery.GetDimensions(sceneW, sceneH);
        uint stnW,   stnH;   gStation.GetDimensions(stnW, stnH);

        // 枕木を 1 本通過するごとに 1 回跳ねる。速度を変えても周期が勝手に追従する
        float phase = TWO_PI * dist / (float)railW;
        float bob   = (sin(phase) - 0.5f) * bobAmp * uiScale;
        float tilt  = sin(phase + 1.1f) * radians(tiltDegrees);

        // ---- 奥の景色（ゆっくり流れる） ----
        {
            float lx = fmod(pix.x / uiScale + dist * parallax, (float)sceneW);
            float ly = (pix.y - (railTop + sceneryDrop * uiScale)) / uiScale + (float)sceneH;
            float4 c = LoadSprite(gScenery, float2(lx, ly));
            color = lerp(color, c.rgb, c.a * screenAlpha);
        }

        // ---- レール（剰余で無限スクロール） ----
        {
            float lx = fmod(pix.x / uiScale + dist, (float)railW);
            float ly = (pix.y - railTop) / uiScale;
            float4 c = LoadSprite(gRail, float2(lx, ly));
            color = lerp(color, c.rgb, c.a * screenAlpha);
        }

        float cartLeft = cartX * (float)screenWidth;

        // ---- 駅：進捗ぶんだけ右から近づく（これが進捗ゲージそのもの） ----
        {
            float goal   = cartLeft + stationGoal * uiScale;
            float startX = (float)screenWidth + kStationEnter * uiScale;
            float sx     = lerp(startX, goal, saturate(progress));
            float2 local = float2((pix.x - sx) / uiScale,
                                  (pix.y - (railTop + stationDrop * uiScale)) / uiScale + (float)stnH);
            float4 c = LoadSprite(gStation, local);
            color = lerp(color, c.rgb, c.a * screenAlpha);
        }

        // ---- トロッコ本体（上下に跳ねながら前後に傾く） ----
        {
            float  cartTop = railTop - (cartLift + (float)cartH) * uiScale + bob;
            float2 half    = float2((float)cartW, (float)cartH) * 0.5f * uiScale;
            float2 center  = float2(cartLeft, cartTop) + half;
            float2 local   = (Rotate(pix - center, -tilt) + half) / uiScale;
            float4 c = LoadSprite(gCart, local);
            color = lerp(color, c.rgb, c.a * screenAlpha);
        }

        // ---- 車輪：車体の後に重ねる。ω = v / r で転がすので滑って見えない ----
        {
            float cy  = railTop - wheelDrop * uiScale + bob;
            float r   = wheelRadius * uiScale;
            float ang = dist / max(wheelRadius, 1.0f);

            [unroll]
            for (int i = 0; i < 2; ++i)
            {
                float inset = (i == 0) ? wheelInset : ((float)cartW - wheelInset);
                float wx    = cartLeft + inset * uiScale;
                color = DrawWheel(color, pix - float2(wx, cy), r, ang, screenAlpha);
            }
        }
    }

    gOutput[dispatchId.xy] = float4(color, 1.0f);
}

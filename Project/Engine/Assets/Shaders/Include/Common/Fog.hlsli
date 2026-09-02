#pragma once

/// @file Fog.hlsli
/// @brief 高さフォグの数式（エンジン内で唯一の実体）
/// @details 全画面合成パス（HeightFog.CS.hlsl）も、前方描画（半透明・水面・パーティクル）も
///          このファイルの関数を呼ぶこと。数式が 2 箇所に分かれると不透明と半透明で
///          フォグ量が食い違い、境目が線として見える。
///
/// 密度モデル: rho(y) = density * exp(-heightFalloff * (y - heightRef))
///   heightFalloff = 0 : 高さ非依存 ＝ 古典的な指数距離フォグ
///   heightFalloff > 0 : 高いほど薄い ＝ 地表に溜まる霧
/// レイに沿った光学的深さは解析的に積分できるので、レイマーチは不要。

/// @brief フォグの共有パラメータ（C++ 側 FogConstants と 1 対 1）
/// @note メンバを増減したら FogManager.h の kFogConstantsFields も直すこと。
///       食い違いは CB_BIND_HLSL のリフレクション照合がシェーダーロード時に検出する。
struct FogParameters
{
    float4x4 invViewProj;     ///< View*Projection の逆行列（行ベクトル規約: p' = p * M）
    float3   cameraWorldPos;  ///< カメラのワールド座標 [m]
    float    density;         ///< heightRef における消散係数 [1/m]
    float3   fogColor;        ///< フォグ色（リニア HDR）
    float    heightFalloff;   ///< 高さ方向の減衰率 [1/m]。0 で高さ非依存
    float    heightRef;       ///< density を与える基準高度 [m]
    float    startDistance;   ///< フォグが効き始めるカメラからの距離 [m]
    float    maxOpacity;      ///< フォグの最大濃度 [0,1]。1 未満なら遠景が消えきらない
    float    skyDistance;     ///< 背景ピクセルのレイ長 [m]
    uint     applyToSky;      ///< 背景（深度 far）にもフォグを掛けるか
    float3   fogPad;
};

/// @brief 光学的深さの上限。exp(-32) = 1.3e-14 で、これ以上は完全に不透明と区別できない
static const float kFogMaxOpticalDepth = 32.0f;

/// @brief exp の指数の上限。積 a * exp(e) が float の範囲を超えないよう抑える
/// @details exp(40) = 2.4e17。密度側と合わせても 3.4e38 に届かない
static const float kFogMaxExponent = 40.0f;

/// @brief レイ origin + t*dir の区間 [t0, t1] における光学的深さを解析積分する
/// @param p      フォグパラメータ
/// @param origin レイ始点（通常はカメラ位置）
/// @param dir    正規化済みレイ方向
/// @param t0     積分開始距離 [m]
/// @param t1     積分終了距離 [m]
/// @return 光学的深さ tau（透過率は exp(-tau)）
/// @details rho(y) = a * exp(-k * (t - t0)) （a = t0 での密度、k = heightFalloff * dir.y）と
///          書けるので、区間積分は a * (1 - exp(-k*s)) / k。k -> 0 の極限は a * s。
float FogOpticalDepth(FogParameters p, float3 origin, float3 dir, float t0, float t1)
{
    const float s = max(t1 - t0, 0.0f);
    if (s <= 0.0f || p.density <= 0.0f)
    {
        return 0.0f;
    }

    // 区間始点の高度における密度
    const float y0 = origin.y + dir.y * t0;
    const float startExponent = clamp(
        -p.heightFalloff * (y0 - p.heightRef), -kFogMaxExponent, kFogMaxExponent);
    const float a = p.density * exp(startExponent);

    const float k = p.heightFalloff * dir.y;

    // ほぼ水平なレイ、または heightFalloff = 0 は一様密度として積分する
    // （このとき a * (1 - exp(-k*s)) / k は 0/0 になる）
    if (abs(k * s) < 1.0e-4f)
    {
        return min(a * s, kFogMaxOpticalDepth);
    }

    // 下向きレイ（k < 0）は exp が発散しうるので指数を打ち止める。
    // 打ち止めた時点で tau は上限を超えているため、結果は変わらない
    const float exponent = min(-k * s, kFogMaxExponent);
    return min(a * (1.0f - exp(exponent)) / k, kFogMaxOpticalDepth);
}

/// @brief 区間 [t0, t1] の透過率を返す（1 = フォグなし、0 = 完全にフォグ色）
float FogTransmittance(FogParameters p, float3 origin, float3 dir, float t0, float t1)
{
    const float tau = FogOpticalDepth(p, origin, dir, t0, t1);
    return lerp(1.0f, exp(-tau), saturate(p.maxOpacity));
}

/// @brief カメラから worldPos までの区間の透過率を返す
float FogTransmittanceToPoint(FogParameters p, float3 worldPos)
{
    const float3 toSurface = worldPos - p.cameraWorldPos;
    const float distance = length(toSurface);
    if (distance < 1.0e-5f)
    {
        return 1.0f;
    }

    const float3 dir = toSurface / distance;
    const float t0 = min(p.startDistance, distance);
    return FogTransmittance(p, p.cameraWorldPos, dir, t0, distance);
}

/// @brief ワールド座標が分かっている面の色へフォグを合成する
/// @details 前方描画（半透明・水面・パーティクル）はこれを呼ぶ。
///          全画面パスと同じ数式を通るので、不透明との境目が出ない。
float3 ApplyFog(FogParameters p, float3 worldPos, float3 color)
{
    return lerp(p.fogColor, color, FogTransmittanceToPoint(p, worldPos));
}

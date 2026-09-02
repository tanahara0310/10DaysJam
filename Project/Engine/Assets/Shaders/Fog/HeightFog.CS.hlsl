/// @file HeightFog.CS.hlsl
/// @brief 高さフォグを SceneColor へ in-place 合成する
/// @details SceneDepth からワールド座標を復元し、カメラからその点までの透過率で
///          シーン色をフォグ色へ寄せる。式: 出力 = lerp(fogColor, sceneColor, transmittance)。
///          gOutput は SceneColor 自身。各スレッドが自分のテクセルだけを読んで書き戻すので
///          中間バッファは要らない（GodRayComposite.CS.hlsl と同じ形）。
///
/// 背景（深度 far）は「skyDistance まで進むレイ」として扱う。
/// heightFalloff > 0 なら上向きレイは光学的深さが収束するので空は自然に薄く、
/// 下向きレイ（地面の穴・世界の縁）は発散するのでフォグ色で埋まる。

#include "Fog.hlsli"
#include "DepthReconstruction.hlsli"

ConstantBuffer<FogParameters> gFog : register(b0);
Texture2D<float> gSceneDepth : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    gOutput.GetDimensions(width, height);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height)
    {
        return;
    }

    const float ndcDepth = gSceneDepth[dispatchThreadId.xy];
    const bool isBackground = IsBackgroundDepth(ndcDepth);

    // 空へ掛けない設定なら背景ピクセルには触れない
    if (isBackground && gFog.applyToSky == 0)
    {
        return;
    }

    const float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(width, height);
    const float3 worldPos = ReconstructWorldPosition(
        ScreenUVToNDC(uv), ndcDepth, gFog.invViewProj);

    const float3 toSurface = worldPos - gFog.cameraWorldPos;
    const float surfaceDistance = length(toSurface);
    if (surfaceDistance < 1.0e-5f)
    {
        return;
    }
    const float3 rayDirection = toSurface / surfaceDistance;

    // 背景の復元点はファークリップ上なので、レイ長は skyDistance で置き換える
    const float rayEnd = isBackground ? max(gFog.skyDistance, 0.0f) : surfaceDistance;
    const float rayStart = min(gFog.startDistance, rayEnd);

    const float transmittance = FogTransmittance(
        gFog, gFog.cameraWorldPos, rayDirection, rayStart, rayEnd);

    const float4 sceneColor = gOutput[dispatchThreadId.xy];
    gOutput[dispatchThreadId.xy] = float4(
        lerp(gFog.fogColor, sceneColor.rgb, transmittance), sceneColor.a);
}

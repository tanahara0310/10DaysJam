#include "MsdfText.hlsli"

// MSDF テキスト専用 VS
//   頂点は「em 単位」で組んである（フォントサイズ 1.0 のときの大きさ）。
//   フォントサイズは WVP のスケール成分として掛かるので、
//   サイズを変えても頂点バッファを組み直す必要が無い。
cbuffer TransformationMatrix : register(b1)
{
    float4x4 WVP;
    float4x4 World;
}

// 頂点は毎フレーム UploadRing へ積み直すので、要素は必要最小限にする。
// （法線・接線を持つ汎用 VertexData のままだと 1 頂点 48B → 24B の倍の転送量になる）
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, WVP);
    output.texcoord = input.texcoord;
    return output;
}

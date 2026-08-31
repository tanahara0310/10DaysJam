#include "MsdfText.hlsli"

ConstantBuffer<TextMaterial> gMaterial : register(b0);

Texture2D<float4> gAtlas  : register(t0);
SamplerState      gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ------------------------------------------------------------
// MSDF の復元は中央値ひとつで終わる。
//
// アトラスの RGB には「それぞれ別のエッジ群だけを見た距離場」が入っている。
// 辺の途中では 3ch のうち 2 つが同じ正しい距離を持ち、コーナーでは 2 本の
// エッジの距離場が交差する。中央値を取ると、辺では正しい距離が、
// コーナーではその交点＝鋭い角が復元される。
// （A チャンネルには真の SDF が入っている。縁取り・グロー用の予約枠で、
//   ここでは使わない。median は角の復元には正しいが距離としては不正確なので、
//   正確な距離が要る効果は A を使うこと）
// ------------------------------------------------------------
float Median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

// ------------------------------------------------------------
// テクスチャ空間の pxRange を「今この画素で何画面ピクセルぶんか」へ変換する。
//
// fwidth(uv) は隣の画素との UV 差分＝画面 1px あたりの UV 変化量なので、
// その逆数が「UV 1.0 が画面何 px に伸びているか」になる。
// 拡大率・回転・パースが変わると fwidth も変わるため、
// アンチエイリアス幅が表示サイズへ自動追従する。
// ここを定数にすると、拡大時にボケ、縮小時にジャギる。
// ------------------------------------------------------------
float ScreenPxRange(float2 uv)
{
    float2 unitRange     = gMaterial.pxRange / float2(gMaterial.atlasWidth, gMaterial.atlasHeight);
    float2 screenTexSize = 1.0f / fwidth(uv);

    // 1.0 でクランプしないと、縮小時に AA 幅が 1px を割って文字が消える
    return max(0.5f * dot(unitRange, screenTexSize), 1.0f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 msd = gAtlas.Sample(gSampler, input.texcoord);

    // 0.5 が輪郭。これより大きければ字の内側
    float signedDistance = Median(msd.r, msd.g, msd.b);

    // 画面ピクセル単位の符号付き距離へ直し、±0.5px で 1px 幅の AA を作る
    float screenPxDistance = ScreenPxRange(input.texcoord) * (signedDistance - 0.5f);
    float alpha = saturate(screenPxDistance + 0.5f);

    PixelShaderOutput output;
    output.color = float4(gMaterial.color.rgb, gMaterial.color.a * alpha);
    return output;
}

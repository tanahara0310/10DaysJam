// ============================================================
// MSDF テキスト描画 共通定義
// ============================================================
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    // xy = アトラス UV / z = アトラス配列の何枚目か
    float3 texcoord : TEXCOORD0;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

struct TextMaterial
{
    float4 color;

    // アトラスを焼いたときの距離場の有効範囲（px）
    float pxRange;
    // アトラス 1 枚あたりの画素サイズ
    float atlasWidth;
    float atlasHeight;
    float padding0;
};

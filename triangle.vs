cbuffer GeomBuffer : register(b0)
{
    float4x4 m;
};

cbuffer SceneBuffer : register(b1)
{
    float4x4 vp;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 worldPos = mul(float4(vertex.pos, 1.0f), m);
    result.pos = mul(worldPos, vp);
    result.uv = vertex.uv;
    return result;
}
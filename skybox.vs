cbuffer GeomBuffer : register(b0)
{
    float4x4 m;
    float4 size;
};

cbuffer SceneBuffer : register(b1)
{
    float4x4 vp;
    float4 cameraPos;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float3 pos = cameraPos.xyz + vertex.pos * size.x;

    float4 worldPos = mul(float4(vertex.pos, 1.0f), m);
    result.pos = mul(worldPos, vp);
    result.localPos = vertex.pos;
    return result;
}
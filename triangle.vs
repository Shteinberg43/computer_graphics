struct Light
{
    float4 pos;
    float4 color;
};

cbuffer GeomBuffer : register(b0)
{
    row_major float4x4 m;
    float4 size;
    row_major float4x4 normalM;
    float4 color;
    float4 material;
};

cbuffer SceneBuffer : register(b1)
{
    row_major float4x4 vp;
    float4 cameraPos;
    int4 lightCount;
    Light lights[10];
    float4 ambientColor;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float3 tang : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 norm : TEXCOORD1;
    float3 tang : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 worldPos = mul(float4(vertex.pos, 1.0f), m);
    result.pos = mul(worldPos, vp);
    result.worldPos = worldPos.xyz;
    result.norm = normalize(mul(float4(vertex.norm, 0.0f), normalM).xyz);
    result.tang = normalize(mul(float4(vertex.tang, 0.0f), normalM).xyz);
    result.uv = vertex.uv;
    return result;
}
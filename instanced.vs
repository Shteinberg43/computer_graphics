struct Light
{
    float4 pos;
    float4 color;
};

struct GeomInstData
{
    row_major float4x4 model;
    row_major float4x4 norm;
    float4 shineSpeedTexIdNM;
    float4 posAngle;
};

cbuffer SceneBuffer : register(b1)
{
    row_major float4x4 vp;
    float4 cameraPos;
    int4 lightCount;
    Light lights[10];
    float4 ambientColor;
};

cbuffer GeomBufferInst : register(b2)
{
    GeomInstData geomInst[100];
};

cbuffer GeomBufferInstVis : register(b3)
{
    uint4 ids[100];
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
    nointerpolation uint instanceId : TEXCOORD4;
};

VSOutput vs(VSInput vertex, uint instanceId : SV_InstanceID)
{
    VSOutput result;

    uint idx = ids[instanceId].x;
    GeomInstData item = geomInst[idx];

    float4 worldPos = mul(float4(vertex.pos, 1.0f), item.model);
    result.pos = mul(worldPos, vp);
    result.worldPos = worldPos.xyz;
    result.norm = normalize(mul(float4(vertex.norm, 0.0f), item.norm).xyz);
    result.tang = normalize(mul(float4(vertex.tang, 0.0f), item.norm).xyz);
    result.uv = vertex.uv;
    result.instanceId = idx;

    return result;
}

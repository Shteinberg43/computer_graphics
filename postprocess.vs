struct VSInput
{
    uint vertexId : SV_VertexID;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 pos = float4(0.0f, 0.0f, 0.0f, 1.0f);
    switch (vertex.vertexId)
    {
    case 0:
        pos = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        break;
    case 1:
        pos = float4(-1.0f, 3.0f, 0.0f, 1.0f);
        break;
    default:
        pos = float4(3.0f, -1.0f, 0.0f, 1.0f);
        break;
    }

    result.pos = pos;
    result.uv = float2(pos.x * 0.5f + 0.5f, 0.5f - pos.y * 0.5f);
    return result;
}


static float2 positions[3] =
{
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5),
};

static float3 colors[3] =
{
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VSOutput MainVS(uint Vertex_ID : SV_VertexID)
{
    VSOutput output;
    output.pos = float4(positions[Vertex_ID], 0.0, 1.0);
    output.color = colors[Vertex_ID];

    return output;
}

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput MainFS(VSOutput input)
{
    PSOutput output;
    output.color = float4(input.color, 1.0);
    return output;
}
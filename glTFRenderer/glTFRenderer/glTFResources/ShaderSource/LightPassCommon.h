struct VS_INPUT
{
    float3 pos: POSITION;
    float2 texCoord: TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
    // xyz - position , w - radius 
    float4 PointLightInfo[4];
};
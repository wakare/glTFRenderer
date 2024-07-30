
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VS_INPUT
{
    DECLARE_ATTRIBUTE_LOCATION(float3 pos, POSITION);
    DECLARE_ATTRIBUTE_LOCATION(float2 uv, TEXCOORD);
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    return output;
}
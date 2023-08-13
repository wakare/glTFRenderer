#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"

#ifdef HAS_TEXCOORD 
SamplerState s0 : register(s0);
#endif

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
#ifdef HAS_TEXCOORD 
    // return interpolated color
    output.baseColor = baseColor_texture.Sample(s0, input.texCoord);
#else
    output.baseColor = float4(1.0, 1.0, 1.0, 1.0);
#endif

#ifdef HAS_NORMAL
    #ifdef HAS_TANGENT
    if (using_normal_mapping)
    {
        float3 normal = normalize(2 * normal_texture.Sample(s0, input.texCoord).xyz - 1.0);
        float3 tmpTangent = normalize(mul(worldMat, float4(input.tangent.xyz, 0.0)).xyz);
        float3 bitangent = cross(input.normal, tmpTangent) * input.tangent.w;
        float3 tangent = cross(bitangent, input.normal);
        float3x3 TBN = transpose(float3x3(tangent, bitangent, input.normal));
        output.normal = normalize(mul(worldMat, float4(mul(TBN, normal), 0.0)));
    }
    else
    #endif
    {
        output.normal = float4(input.normal, 0.0);
    }
    output.normal = normalize(output.normal) * 0.5 + 0.5;
#else
    output.normal = float4(0.0, 0.0, 0.0, 0.0);
#endif
    return output;
}
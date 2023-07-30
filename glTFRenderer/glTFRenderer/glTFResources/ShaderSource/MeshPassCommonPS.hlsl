#include "glTFResources/ShaderSource/MeshPassCommon.h"

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
        float3 normal = normal_texture.Sample(s0, input.texCoord).xyz;
        float3 bitangent = normalize(cross(input.normal, input.tangent));
        float3x3 TBN = transpose(float3x3(input.tangent, bitangent, input.normal));
        output.normal = float4(mul(TBN, normal), 0.0);
    }
    else
    #endif
    {
        output.normal = float4(input.normal, 0.0);
    }
#else
    output.normal = float4(0.0, 0.0, 0.0, 0.0);
#endif
    return output;
}
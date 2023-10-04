#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

#ifdef HAS_TEXCOORD 
SamplerState defaultSampler : DEFAULT_SAMPLER_REGISTER_INDEX;
#endif

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
#ifdef HAS_TEXCOORD 
    // return interpolated color
    //output.baseColor = baseColor_texture.Sample(defaultSampler, input.texCoord);
    output.baseColor = SampleAlbedoTexture(material_id, input.texCoord);
    //output.baseColor = GetMaterialDebugColor(material_id);
#else
    output.baseColor = float4(1.0, 1.0, 1.0, 1.0);
#endif

#ifdef HAS_NORMAL
    #ifdef HAS_TANGENT
    if (using_normal_mapping)
    {
        //float3 normal = normalize(2 * normal_texture.Sample(defaultSampler, input.texCoord).xyz - 1.0);
        float3 normal = normalize(2 * SampleNormalTexture(material_id, input.texCoord).xyz - 1.0);
        float3 tmpTangent = normalize(mul(world_matrix, float4(input.tangent.xyz, 0.0)).xyz);
        float3 bitangent = cross(input.normal, tmpTangent) * input.tangent.w;
        float3 tangent = cross(bitangent, input.normal);
        float3x3 TBN = transpose(float3x3(tangent, bitangent, input.normal));
        output.normal = normalize(mul(world_matrix, float4(mul(TBN, normal), 0.0)));
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
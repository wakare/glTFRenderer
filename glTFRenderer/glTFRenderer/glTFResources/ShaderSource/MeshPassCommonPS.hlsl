#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

#ifdef HAS_TEXCOORD 
SamplerState defaultSampler : DEFAULT_SAMPLER_REGISTER_INDEX;
#endif

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
#ifdef HAS_TEXCOORD
    float2 metallic_roughness = SampleMetallicRoughnessTexture(input.vs_material_id, input.texCoord);
    output.baseColor = SampleAlbedoTexture(input.vs_material_id, input.texCoord);
#else
    float2 metallic_roughness = SampleMetallicRoughnessTexture(input.vs_material_id, float2(0.0, 0.0));
    output.baseColor = SampleAlbedoTexture(input.vs_material_id,float2(0.0, 0.0));
#endif

#ifdef HAS_NORMAL
    #ifdef HAS_TANGENT
    if (input.normal_mapping)
    {
        float3 normal = normalize(2 * SampleNormalTexture(input.vs_material_id, input.texCoord).xyz - 1.0);
        output.normal = float4(GetWorldNormal(input.world_rotation_matrix, input.normal, input.tangent, normal), 0.0);
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
    output.baseColor.w = metallic_roughness.x;
    output.normal.w = metallic_roughness.y;
    
    return output;
}
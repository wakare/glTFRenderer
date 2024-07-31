
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"

struct VSOutput
{
    float4 pos: SV_POSITION;
    
#ifdef HAS_NORMAL
    float3 normal: NORMAL;
#endif

#ifdef HAS_TANGENT
    float4 tangent: TANGENT;
#endif
    
#ifdef HAS_TEXCOORD
    float2 texCoord: TEXCOORD;
#endif

    uint vs_material_id: MATERIAL_ID;
    uint normal_mapping: NORMAL_MAPPING;
    float3x3 world_rotation_matrix: WORLD_ROTATION_MATRIX;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input)
{
    PSOutput output;
    output.color = SampleAlbedoTexture(input.vs_material_id, input.texCoord);
    //output.color = float4(1.0, 1.0, 0.0, 1.0);
    
    return output;
}
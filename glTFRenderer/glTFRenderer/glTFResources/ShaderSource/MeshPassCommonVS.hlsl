#include "glTFResources/ShaderSource/MeshPassCommon.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 instance_transform = float4x4(input.instance_matrix_0, input.instance_matrix_1, input.instance_matrix_2, input.instance_matrix_3);
    //float4 world_pos = mul(world_matrix, float4(input.pos, 1.0));
    float4 world_pos = mul(instance_transform, float4(input.pos, 1.0));
    float4 view_pos = mul(viewMatrix, world_pos);
    output.pos = mul (projectionMatrix, view_pos);
#ifdef HAS_NORMAL
    output.normal = normalize(mul(world_matrix, float4(input.normal, 0.0)).xyz);
#endif

#ifdef HAS_TANGENT
    output.tangent = input.tangent;
#endif
    
#ifdef HAS_TEXCOORD 
    output.texCoord = input.texCoord;
#endif
    return output;
}
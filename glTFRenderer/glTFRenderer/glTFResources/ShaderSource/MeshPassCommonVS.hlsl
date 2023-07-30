#include "glTFResources/ShaderSource/MeshPassCommon.h"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 world_pos = mul(worldMat, float4(input.pos, 1.0));
    float4 view_pos = mul(viewMatrix, world_pos);
    output.pos = mul (projectionMatrix, view_pos);
#ifdef HAS_NORMAL
    output.normal = mul(transInvWorldMat, float4(input.normal, 0.0)).xyz * 0.5 + 0.5;
#endif

#ifdef HAS_TANGENT
    output.tangent = input.tangent;
#endif
    
#ifdef HAS_TEXCOORD 
    output.texCoord = input.texCoord;
#endif
    return output;
}
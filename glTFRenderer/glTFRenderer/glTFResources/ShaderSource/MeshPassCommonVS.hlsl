#include "glTFResources/ShaderSource/MeshPassCommon.h"


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = mul (viewProjectionMat, mul(worldMat, float4(input.pos, 1.0f)));
    output.texCoord = input.texCoord;
    return output;
}
#include "glTFResources/ShaderSource/LightPassCommon.h"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}
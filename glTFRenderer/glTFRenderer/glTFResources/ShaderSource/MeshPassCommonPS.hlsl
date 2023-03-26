#include "glTFResources/ShaderSource/MeshPassCommon.h"

Texture2D t0: register(t0);
SamplerState s0 : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
    // return interpolated color
    return t0.Sample(s0, input.texCoord);
}
#include "glTFResources/ShaderSource/MeshPassCommon.h"

#ifdef HAS_TEXCOORD 
Texture2D t0: register(t0);
SamplerState s0 : register(s0);
#endif

float4 main(VS_OUTPUT input) : SV_TARGET
{
#ifdef HAS_TEXCOORD 
    // return interpolated color
    return t0.Sample(s0, input.texCoord);
#else
    return float4(1.0, 1.0, 1.0, 1.0);
#endif
}
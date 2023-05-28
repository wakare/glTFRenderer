#include "glTFResources/ShaderSource/MeshPassCommon.h"

#ifdef HAS_TEXCOORD 
Texture2D t0: register(t0);
SamplerState s0 : register(s0);
#endif

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
#ifdef HAS_TEXCOORD 
    // return interpolated color
    output.baseColor = t0.Sample(s0, input.texCoord);
#else
    output.baseColor = float4(1.0, 1.0, 1.0, 1.0);
#endif

#ifdef HAS_NORMAL
    output.normal = float4(input.normal, 0.0);
#else
    output.normal = float4(0.0, 0.0, 0.0, 0.0);
#endif
    return output;
}
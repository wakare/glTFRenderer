#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.pos = float4(input.pos, 1.0f);
#ifdef HAS_NORMAL
    output.normal = input.normal;
#endif
    
#ifdef HAS_TEXCOORD 
    output.texCoord = input.texCoord;
#endif
    return output;
}
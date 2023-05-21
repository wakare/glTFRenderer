#include "glTFResources/ShaderSource/LightPassCommon.h"

Texture2D albedoTex: register(t0);
SamplerState albedoTexSampler : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{   
    return albedoTex.Sample(albedoTexSampler, input.texCoord);
}
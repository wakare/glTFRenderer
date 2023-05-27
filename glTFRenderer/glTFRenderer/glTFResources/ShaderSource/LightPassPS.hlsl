#include "glTFResources/ShaderSource/LightPassCommon.h"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
SamplerState defaultSampler : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{   
    return albedoTex.Sample(defaultSampler, input.texCoord) * pow(depthTex.Sample(defaultSampler, input.texCoord).r, 32.0);
}
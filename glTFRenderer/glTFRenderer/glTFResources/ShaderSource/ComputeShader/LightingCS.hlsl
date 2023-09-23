#include "glTFResources/ShaderSource/Interface/LightingInterface.hlsl"
#include "glTFResources/ShaderSource/LightPassCommon.hlsl"

Texture2D albedoTex: register(t0);
Texture2D depthTex: register(t1);
Texture2D normalTex: register(t2);

RWTexture2D<float4> Output: register(u0) ;

SamplerState defaultSampler : register(s0);

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    //Output[dispatchThreadID.xy] = float4(0.5, 0.5, 1.0, 1.0);
    Output[dispatchThreadID.xy] = albedoTex.Load(dispatchThreadID.x, dispatchThreadID.y);
}
#ifndef LIGHT_PASS_COMMON
#define LIGHT_PASS_COMMON
#include "glTFResources/ShaderSource/PassCommon.hlsl"
#include "glTFResources/ShaderSource/SceneView.hlsl"

struct VS_INPUT
{
    float3 pos: POSITION;
    
#ifdef HAS_NORMAL 
    float3 normal: NORMAL;
#endif
    
#ifdef HAS_TEXCOORD 
    float2 texCoord: TEXCOORD;
#endif
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    
#ifdef HAS_NORMAL
    float3 normal: NORMAL;
#endif
    
#ifdef HAS_TEXCOORD
    float2 texCoord: TEXCOORD;
#endif
};

#endif


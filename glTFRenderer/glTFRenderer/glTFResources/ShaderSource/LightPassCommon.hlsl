#ifndef LIGHT_PASS_COMMON
#define LIGHT_PASS_COMMON

#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"

struct VS_INPUT
{
    DECLARE_ATTRIBUTE_LOCATION(float3 pos, POSITION);
    
#ifdef HAS_NORMAL 
    DECLARE_ATTRIBUTE_LOCATION(float3 normal, NORMAL);
#endif
    
#ifdef HAS_TEXCOORD 
    DECLARE_ATTRIBUTE_LOCATION(float2 texCoord, TEXCOORD);
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


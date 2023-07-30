#include "glTFResources/ShaderSource/SceneView.h"
#include "glTFResources/ShaderSource/SceneMesh.h"

struct VS_INPUT
{
    float3 pos: POSITION;
    
#ifdef HAS_NORMAL 
    float3 normal: NORMAL;
#endif
    
#ifdef HAS_TANGENT
    float3 tangent: TANGENT;
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

#ifdef HAS_TANGENT
    float3 tangent: TANGENT;
#endif
    
#ifdef HAS_TEXCOORD
    float2 texCoord: TEXCOORD;
#endif
};

#define PS_INPUT VS_OUTPUT

struct PS_OUTPUT
{
    float4 baseColor: SV_TARGET0;
    float4 normal: SV_TARGET1;
};
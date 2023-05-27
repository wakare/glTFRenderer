#include "glTFResources/ShaderSource/SceneView.h"
#include "glTFResources/ShaderSource/SceneMesh.h"

struct VS_INPUT
{
    float3 pos: POSITION;
    float2 texCoord: TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};
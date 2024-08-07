//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"

struct RayPayload
{
    float4 color;
};

struct HitGroupCB
{
    uint material_id;
};

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;
ConstantBuffer<HitGroupCB> hit_group_cb : MATERIAL_ID_REGISTER_INDEX;

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

static float4 debug_colors[8] =
{
    float4(1.0, 0.0, 0.0, 1.0),
    float4(0.0, 1.0, 0.0, 1.0),
    float4(0.0, 0.0, 1.0, 1.0),
    float4(1.0, 1.0, 0.0, 1.0),
    float4(1.0, 0.0, 1.0, 1.0),
    float4(1.0, 1.0, 1.0, 1.0),
    float4(0.0, 1.0, 1.0, 1.0),
    float4(0.0, 0.0, 0.0, 1.0),
};

[shader("raygeneration")]
void MyRaygenShader()
{
    float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();
    // Orthographic projection since we're raytracing in screen space.
    float3 rayOffsetDir = float3(lerpValues.x - 0.5, 0.5 - lerpValues.y, 1.0);
    float4 rayWorldDir = float4(rayOffsetDir, 0.0);
    rayWorldDir = normalize(mul(inverseViewMatrix, rayWorldDir)); 
    
    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = view_position.xyz;
    ray.Direction = rayWorldDir.xyz;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000000.0;
    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    // Write the raytraced color to the output texture.
    render_target[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    uint3 vertex_indices = GetMeshTriangleVertexIndex(InstanceID(), PrimitiveIndex());
    float2 uv = InterpolateVertexWithBarycentrics(vertex_indices, attr.barycentrics).uv;
    payload.color = SampleAlbedoTextureCS(hit_group_cb.material_id, uv);
    //payload.color = debug_colors[hit_group_cb.material_id % 8];
    //payload.color = float4(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y, 1.0);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

#endif // RAYTRACING_HLSL
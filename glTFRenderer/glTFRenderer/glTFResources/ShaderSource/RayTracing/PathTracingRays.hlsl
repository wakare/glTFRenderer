#ifndef PATH_TRACING_RAYS
#define PATH_TRACING_RAYS

#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

#define PATH_TRACING_RAY_COUNT 2
#define PATH_TRACING_RAY_INDEX_PRIMARY 0
#define PATH_TRACING_RAY_INDEX_SHADOW 1

struct HitGroupCB
{
    uint material_id;
};

ConstantBuffer<HitGroupCB> hit_group_cb : MATERIAL_ID_REGISTER_INDEX;

typedef BuiltInTriangleIntersectionAttributes PathTracingAttributes;

// --------------- Primary Ray --------------------

struct PrimaryRayPayload
{
    float4 normal;
    float4 albedo;
    
    float distance;
    uint material_id;
};

bool IsHit(PrimaryRayPayload payload)
{
    return payload.distance > 0.0;
}

[shader("closesthit")]
void PrimaryRayClosestHit(inout PrimaryRayPayload payload, in PathTracingAttributes attr)
{
    payload.distance = RayTCurrent();
    payload.material_id = hit_group_cb.material_id;
    SceneMeshVertexInfo vertex_info = InterpolateVertexWithBarycentrics(
        GetMeshTriangleVertexIndex(InstanceID(), PrimitiveIndex()), attr.barycentrics);

    float4 tangent_normal = SampleNormalTextureCS(payload.material_id, vertex_info.uv.xy);
    float3x4 world_matrix = ObjectToWorld();
    payload.normal = float4(GetWorldNormal((float3x3)world_matrix, vertex_info.normal.xyz, vertex_info.tangent, tangent_normal.xyz), 0.0);

    payload.albedo = SampleAlbedoTextureCS(payload.material_id, vertex_info.uv.xy);
}

[shader("miss")]
void PrimaryRayMiss(inout PrimaryRayPayload payload)
{
    payload.distance = -1.0f;   
}

void TracePrimaryRay(in RaytracingAccelerationStructure tlas, in RayDesc ray, inout PrimaryRayPayload payload)
{
    TraceRay(
            tlas,
            RAY_FLAG_FORCE_OPAQUE,
            ~0,
            PATH_TRACING_RAY_INDEX_PRIMARY,
            PATH_TRACING_RAY_COUNT,
            PATH_TRACING_RAY_INDEX_PRIMARY,
            ray,
            payload);
}

// --------------- Shadow Ray --------------------

struct ShadowRayPayload
{
    bool hit;
};

[shader("miss")]
void ShadowRayMiss(inout ShadowRayPayload payload)
{
    // No hit
    payload.hit = false;
}

bool TraceShadowRay(in RaytracingAccelerationStructure tlas, in RayDesc ray)
{
    ShadowRayPayload payload;
    payload.hit = true;
    
    TraceRay(
            tlas,
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            ~0,
            PATH_TRACING_RAY_INDEX_SHADOW,
            0,
            PATH_TRACING_RAY_INDEX_SHADOW,
            ray,
            payload);

    return payload.hit;
}
#endif
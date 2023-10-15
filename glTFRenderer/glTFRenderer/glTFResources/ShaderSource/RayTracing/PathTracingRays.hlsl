#ifndef PATH_TRACING_RAYS
#define PATH_TRACING_RAYS

#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"

#define PATH_TRACING_RAY_COUNT 1
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
    float distance;
    uint material_id;
    float2 uv;
    float debug;
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
    payload.uv = InterpolateVertexWithBarycentrics(
        GetMeshTriangleVertexIndex(InstanceID(), PrimitiveIndex()), attr.barycentrics).uv;
    payload.debug = 0.5;
    //payload.material_id = InstanceID();
}

[shader("miss")]
void PrimaryRayMiss(inout PrimaryRayPayload payload)
{
    payload.distance = -1.0f;
    payload.material_id = 0;
    payload.uv = 0.0;
    payload.debug = 0.0;
}

void TracePrimaryRay(in RaytracingAccelerationStructure tlas, in RayDesc ray, inout PrimaryRayPayload payload)
{
    TraceRay(
            tlas,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
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
    float debug;
};

[shader("miss")]
void ShadowRayMiss(inout ShadowRayPayload payload)
{
    // No hit
    payload.hit = false;
    payload.debug = 0.5;
}

bool TraceShadowRay(RaytracingAccelerationStructure tlas, RayDesc ray)
{
    ShadowRayPayload payload;
    payload.hit = true;
    payload.debug = 0.0;
    
    TraceRay(
            tlas,
            RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
            ~0,
            PATH_TRACING_RAY_INDEX_SHADOW,
            PATH_TRACING_RAY_COUNT,
            PATH_TRACING_RAY_INDEX_SHADOW,
            ray,
            payload);

    return payload.hit;
}
#endif
#ifndef PATH_TRACING_RAYS
#define PATH_TRACING_RAYS

#include "glTFResources/ShaderSource/Interface/SceneMeshInfo.hlsl"
#include "glTFResources/ShaderSource/Interface/SceneMaterial.hlsl"
#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

#define PATH_TRACING_RAY_COUNT 2
#define PATH_TRACING_RAY_INDEX_PRIMARY 0
#define PATH_TRACING_RAY_INDEX_SHADOW 1

typedef BuiltInTriangleIntersectionAttributes PathTracingAttributes;

// --------------- Primary Ray --------------------
struct [raypayload] PrimaryRayPayload
{
    float3 normal : read(caller) : write(closesthit);
    float3 albedo : read(caller) : write(closesthit);
    
    float metallic : read(caller) : write(closesthit);
    float roughness : read(caller) : write(closesthit);
    float distance : read(caller) : write(closesthit, miss);

    uint instance_id : read(caller) : write(closesthit);
    uint primitive_id : read(caller) : write(closesthit);
};

bool IsHit(PrimaryRayPayload payload)
{
    return payload.distance > 0.0;
}

[shader("closesthit")]
void PrimaryRayClosestHit(inout PrimaryRayPayload payload, in PathTracingAttributes attr)
{
    payload.distance = RayTCurrent();
    SceneMeshVertexInfo vertex_info = InterpolateVertexWithBarycentrics(
        GetMeshTriangleVertexIndex(InstanceID(), PrimitiveIndex()), attr.barycentrics);

    MaterialSurfaceInfo surface_info = GetSurfaceInfoCS(GetMeshMaterialId(InstanceID()), vertex_info.uv.xy);
    payload.albedo = surface_info.albedo;
    
    float3x4 world_matrix = ObjectToWorld3x4();
    payload.normal = GetWorldNormal((float3x3)world_matrix, vertex_info.normal.xyz, vertex_info.tangent, surface_info.normal);
    
    payload.metallic = surface_info.metallic;
    payload.roughness = surface_info.roughness;

    payload.instance_id = InstanceID();
    payload.primitive_id = PrimitiveIndex();
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

struct [raypayload] ShadowRayPayload
{
    bool hit : read(caller) : write(miss);
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
#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"
#include "glTFResources/ShaderSource/RayTracing/PathTracingRays.hlsl"

RaytracingAccelerationStructure scene : SCENE_AS_REGISTER_INDEX;
RWTexture2D<float4> render_target : OUTPUT_REGISTER_INDEX;

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
void PathTracingRayGen()
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
    PrimaryRayPayload payload = { float4(0, 0, 0, 0), -1.0f };
    TraceRay(
        scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        ~0,
        PATH_TRACING_RAY_INDEX_PRIMARY,
        PATH_TRACING_RAY_COUNT,
        PATH_TRACING_RAY_INDEX_PRIMARY,
        ray,
        payload);

    // Write the raytraced color to the output texture.
    render_target[DispatchRaysIndex().xy] = payload.color;
}

#endif // RAYTRACING_HLSL
RaytracingAccelerationStructure g_scene_as : register(t0);
RWTexture2D<float4> g_bake_output : register(u0);

struct [raypayload] BakePayload
{
    float4 color : read(caller) : write(caller, miss, closesthit);
};

[shader("raygeneration")]
void BakeRayGenMain()
{
    const uint2 launch_index = DispatchRaysIndex().xy;
    const uint2 launch_extent = DispatchRaysDimensions().xy;
    const float2 uv = (float2(launch_index) + 0.5f) / max(float2(launch_extent), 1.0.xx);

    RayDesc ray_desc;
    ray_desc.Origin = float3(lerp(-1.0, 1.0, uv.x), lerp(1.0, -1.0, uv.y), 2.0);
    ray_desc.Direction = float3(0.0, 0.0, -1.0);
    ray_desc.TMin = 0.001;
    ray_desc.TMax = 10000.0;

    BakePayload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    TraceRay(
        g_scene_as,
        RAY_FLAG_NONE,
        0xff,
        0,
        1,
        0,
        ray_desc,
        payload);

    g_bake_output[launch_index] = payload.color;
}

[shader("miss")]
void BakeMissMain(inout BakePayload payload)
{
    payload.color = float4(0.05, 0.08, 0.18, 1.0);
}

[shader("closesthit")]
void BakeClosestHitMain(inout BakePayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    const float3 barycentric = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y,
                                      attributes.barycentrics.x,
                                      attributes.barycentrics.y);
    payload.color = float4(barycentric, 1.0);
}

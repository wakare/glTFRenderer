RaytracingAccelerationStructure g_scene_as : register(t0);

struct BakeTexelRecord
{
    float4 world_position_and_valid;
    float4 geometric_normal_and_padding;
};

StructuredBuffer<BakeTexelRecord> g_bake_texel_records : register(t1);
RWTexture2D<float4> g_bake_output : register(u0);

cbuffer g_bake_dispatch_constants : register(b0)
{
    uint atlas_width;
    uint atlas_height;
    uint sample_index;
    uint sample_count;
    uint max_bounces;
    float ray_epsilon;
    float sky_intensity;
    float sky_ground_mix;
};

struct [raypayload] BakePayload
{
    float4 color : read(caller) : write(caller, miss, closesthit);
};

static const float kPi = 3.14159265359f;

uint Hash32(uint value)
{
    value += 0x9E3779B9u;
    value = (value ^ (value >> 16u)) * 0x85EBCA6Bu;
    value = (value ^ (value >> 13u)) * 0xC2B2AE35u;
    return value ^ (value >> 16u);
}

float Random01(inout uint state)
{
    state = Hash32(state);
    return (float)(state & 0x00FFFFFFu) / 16777216.0f;
}

void BuildOrthonormalBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
    const float sign_value = normal.z >= 0.0f ? 1.0f : -1.0f;
    const float a = -1.0f / (sign_value + normal.z);
    const float b = normal.x * normal.y * a;
    tangent = float3(1.0f + sign_value * normal.x * normal.x * a, sign_value * b, -sign_value * normal.x);
    bitangent = float3(b, sign_value + normal.y * normal.y * a, -normal.y);
}

float3 SampleCosineHemisphere(float2 sample_uv)
{
    const float radius = sqrt(sample_uv.x);
    const float phi = 2.0f * kPi * sample_uv.y;
    const float x = radius * cos(phi);
    const float y = radius * sin(phi);
    const float z = sqrt(max(0.0f, 1.0f - sample_uv.x));
    return float3(x, y, z);
}

float3 EvaluateSkyRadiance(float3 world_direction)
{
    const float up = saturate(world_direction.y * 0.5f + 0.5f);
    const float horizon = saturate(1.0f - abs(world_direction.y));
    const float3 sky_top = float3(0.52f, 0.66f, 0.84f);
    const float3 sky_bottom = float3(0.08f, 0.10f, 0.14f);
    const float3 horizon_tint = 0.08f * horizon * float3(1.0f, 0.92f, 0.78f);
    const float3 sky = lerp(sky_bottom, sky_top, up) + horizon_tint;
    const float3 ground = sky_ground_mix * float3(0.06f, 0.05f, 0.04f);
    return lerp(ground, sky, up) * sky_intensity;
}

[shader("raygeneration")]
void BakeRayGenMain()
{
    const uint2 launch_index = DispatchRaysIndex().xy;
    if (launch_index.x >= atlas_width || launch_index.y >= atlas_height)
    {
        return;
    }

    const uint pixel_index = launch_index.y * atlas_width + launch_index.x;
    const BakeTexelRecord texel_record = g_bake_texel_records[pixel_index];
    if (texel_record.world_position_and_valid.w < 0.5f)
    {
        g_bake_output[launch_index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    if (sample_count == 0u)
    {
        g_bake_output[launch_index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 geometric_normal = texel_record.geometric_normal_and_padding.xyz;
    if (dot(geometric_normal, geometric_normal) < 1e-8f)
    {
        geometric_normal = float3(0.0f, 1.0f, 0.0f);
    }

    geometric_normal = normalize(geometric_normal);

    float3 tangent;
    float3 bitangent;
    BuildOrthonormalBasis(geometric_normal, tangent, bitangent);

    float3 radiance_sum = float3(0.0f, 0.0f, 0.0f);
    for (uint sample_offset = 0u; sample_offset < sample_count; ++sample_offset)
    {
        const uint current_sample_index = sample_index + sample_offset;
        uint rng_state = Hash32(pixel_index ^ (current_sample_index * 0xA511E9B3u) ^ (max_bounces * 0x63D83595u));
        const float2 sample_uv = float2(Random01(rng_state), Random01(rng_state));
        const float3 local_direction = SampleCosineHemisphere(sample_uv);
        const float3 world_direction = normalize(
            local_direction.x * tangent +
            local_direction.y * bitangent +
            local_direction.z * geometric_normal);

        RayDesc ray_desc;
        ray_desc.Origin = texel_record.world_position_and_valid.xyz + geometric_normal * ray_epsilon;
        ray_desc.Direction = world_direction;
        ray_desc.TMin = ray_epsilon;
        ray_desc.TMax = 10000.0f;

        BakePayload payload;
        payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

        TraceRay(
            g_scene_as,
            RAY_FLAG_NONE,
            0xff,
            0,
            1,
            0,
            ray_desc,
            payload);

        radiance_sum += payload.color.rgb;
    }

    g_bake_output[launch_index] = float4(radiance_sum / max((float)sample_count, 1.0f), 1.0f);
}

[shader("miss")]
void BakeMissMain(inout BakePayload payload)
{
    payload.color = float4(EvaluateSkyRadiance(normalize(WorldRayDirection())), 1.0f);
}

[shader("closesthit")]
void BakeClosestHitMain(inout BakePayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
}

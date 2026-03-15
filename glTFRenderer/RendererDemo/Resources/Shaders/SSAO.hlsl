#include "SceneViewCommon.hlsl"

#define USE_PCG 0
#include "Math/RandomGenerator.hlsl"
#include "Math/Sample.hlsl"

Texture2D<float4> normalTex;
Texture2D<float> depthTex;
Texture2D<float> InputAOTex;

RWTexture2D<float> Output;

cbuffer SSAOGlobalBuffer
{
    float radius_world;
    float intensity;
    float power;
    float bias;
    float thickness;
    float sample_distribution_power;
    float blur_depth_reject;
    float blur_normal_reject;
    uint sample_count;
    uint blur_radius;
    uint enabled;
    uint pad0;
};

static const float kClearDepthEpsilon = 1e-5f;

uint2 GetViewportExtent()
{
    return uint2(max(viewport_width, 1u), max(viewport_height, 1u));
}

bool IsDepthValid(float depth)
{
    return depth < (1.0f - kClearDepthEpsilon);
}

float3 DecodeNormal(float4 encoded_normal)
{
    return normalize(encoded_normal.xyz * 2.0f - 1.0f);
}

float3 GetWorldPosition(uint2 texel, float depth, out bool valid_world_position)
{
    const float2 viewport_size = max(float2((float)viewport_width, (float)viewport_height), float2(1.0f, 1.0f));
    const float2 uv = (float2(texel) + 0.5f) / viewport_size;
    const float4 clip_space_coord = float4(2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y, depth, 1.0f);
    float4 world_space_coord = mul(inverse_view_projection_matrix, clip_space_coord);
    if (abs(world_space_coord.w) < 1e-6f)
    {
        valid_world_position = false;
        return 0.0f;
    }

    world_space_coord /= world_space_coord.w;
    valid_world_position = true;
    return world_space_coord.xyz;
}

bool ProjectWorldPositionToUV(float3 world_position, out float2 out_uv)
{
    float4 clip_position = mul(view_projection_matrix, float4(world_position, 1.0f));
    if (abs(clip_position.w) < 1e-6f)
    {
        out_uv = 0.0f;
        return false;
    }

    clip_position.xyz /= clip_position.w;
    out_uv = float2(
        clip_position.x * 0.5f + 0.5f,
        0.5f - clip_position.y * 0.5f);
    return all(out_uv >= float2(0.0f, 0.0f)) && all(out_uv <= float2(1.0f, 1.0f));
}

float3 MakeFallbackTangent(float3 normal)
{
    const float3 fallback_axis = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(fallback_axis, normal));
}

float ComputeSSAO(uint2 pixel)
{
    if (enabled == 0u)
    {
        return 1.0f;
    }

    const float depth = depthTex.Load(int3(pixel, 0));
    if (!IsDepthValid(depth))
    {
        return 1.0f;
    }

    const float4 encoded_normal = normalTex.Load(int3(pixel, 0));
    if (dot(encoded_normal.xyz, encoded_normal.xyz) < 1e-8f)
    {
        return 1.0f;
    }

    bool valid_world_position = false;
    const float3 world_position = GetWorldPosition(pixel, depth, valid_world_position);
    if (!valid_world_position)
    {
        return 1.0f;
    }

    const float3 normal = DecodeNormal(encoded_normal);
    const uint2 viewport_extent = GetViewportExtent();

    RngStateType rng = initRNG(pixel, viewport_extent, 0u);
    float3 random_vector = normalize(float3(
        rand(rng) * 2.0f - 1.0f,
        rand(rng) * 2.0f - 1.0f,
        rand(rng) * 2.0f - 1.0f));

    float3 tangent = random_vector - normal * dot(random_vector, normal);
    if (dot(tangent, tangent) < 1e-6f)
    {
        tangent = MakeFallbackTangent(normal);
    }
    else
    {
        tangent = normalize(tangent);
    }
    const float3 bitangent = normalize(cross(normal, tangent));

    const uint clamped_sample_count = clamp(sample_count, 1u, 32u);
    const float clamped_radius = max(radius_world, 1e-4f);
    const float clamped_bias = max(bias, 1e-5f);
    const float clamped_thickness = max(thickness, 1e-4f);
    const float clamped_distribution_power = max(sample_distribution_power, 0.25f);

    float occlusion_sum = 0.0f;
    uint valid_sample_count = 0u;

    [loop]
    for (uint sample_index = 0u; sample_index < clamped_sample_count; ++sample_index)
    {
        const float sample_ratio = (float(sample_index) + 0.5f) / float(clamped_sample_count);
        const float sample_scale = lerp(0.10f, 1.0f, pow(sample_ratio, clamped_distribution_power));
        const float sample_distance = clamped_radius * sample_scale;
        const float2 xi = float2(
            frac(sample_ratio + rand(rng)),
            rand(rng));

        float pdf = 0.0f;
        const float3 hemisphere = sampleHemisphereCosine(xi, pdf);
        float3 sample_direction = tangent * hemisphere.x + bitangent * hemisphere.y + normal * hemisphere.z;
        if (dot(sample_direction, sample_direction) < 1e-8f)
        {
            continue;
        }
        sample_direction = normalize(sample_direction);

        float2 sample_uv = 0.0f;
        if (!ProjectWorldPositionToUV(world_position + sample_direction * sample_distance, sample_uv))
        {
            continue;
        }

        uint2 sample_pixel = min((uint2)(sample_uv * float2(viewport_extent)), viewport_extent - uint2(1u, 1u));
        if (all(sample_pixel == pixel))
        {
            continue;
        }

        const float sample_depth = depthTex.Load(int3(sample_pixel, 0));
        if (!IsDepthValid(sample_depth))
        {
            continue;
        }

        bool valid_sample_world_position = false;
        const float3 sample_world_position = GetWorldPosition(sample_pixel, sample_depth, valid_sample_world_position);
        if (!valid_sample_world_position)
        {
            continue;
        }

        const float3 sample_delta = sample_world_position - world_position;
        const float along_ray = dot(sample_delta, sample_direction);
        const float along_normal = dot(sample_delta, normal);
        if (along_ray <= clamped_bias || along_normal <= clamped_bias)
        {
            continue;
        }

        valid_sample_count += 1u;
        if (along_ray >= sample_distance)
        {
            continue;
        }

        const float occlusion_gap = sample_distance - along_ray;
        const float distance_weight = 1.0f - saturate(sample_distance / clamped_radius);
        const float thickness_weight = smoothstep(0.0f, clamped_thickness, occlusion_gap);
        const float angle_weight = saturate(dot(normalize(sample_delta), normal));
        occlusion_sum += distance_weight * thickness_weight * angle_weight;
    }

    if (valid_sample_count == 0u)
    {
        return 1.0f;
    }

    const float normalized_occlusion = saturate((occlusion_sum / float(valid_sample_count)) * intensity);
    return saturate(pow(1.0f - normalized_occlusion, max(power, 1e-4f)));
}

float ComputeBlurredSSAO(uint2 pixel)
{
    if (enabled == 0u)
    {
        return 1.0f;
    }

    const float center_depth = depthTex.Load(int3(pixel, 0));
    if (!IsDepthValid(center_depth))
    {
        return 1.0f;
    }

    const float4 center_encoded_normal = normalTex.Load(int3(pixel, 0));
    if (dot(center_encoded_normal.xyz, center_encoded_normal.xyz) < 1e-8f)
    {
        return 1.0f;
    }

    bool valid_center_world_position = false;
    const float3 center_world_position = GetWorldPosition(pixel, center_depth, valid_center_world_position);
    if (!valid_center_world_position)
    {
        return 1.0f;
    }

    const float center_ao = InputAOTex.Load(int3(pixel, 0));
    const float3 center_normal = DecodeNormal(center_encoded_normal);
    const uint2 viewport_extent = GetViewportExtent();
    const int clamped_blur_radius = int(clamp(blur_radius, 0u, 3u));

    float weight_sum = 1.0f;
    float ao_sum = center_ao;

    [loop]
    for (int y = -clamped_blur_radius; y <= clamped_blur_radius; ++y)
    {
        [loop]
        for (int x = -clamped_blur_radius; x <= clamped_blur_radius; ++x)
        {
            if (x == 0 && y == 0)
            {
                continue;
            }

            const int2 sample_offset = int2(x, y);
            const uint2 sample_pixel = uint2(clamp(
                int2(pixel) + sample_offset,
                int2(0, 0),
                int2(viewport_extent) - 1));

            const float sample_depth = depthTex.Load(int3(sample_pixel, 0));
            if (!IsDepthValid(sample_depth))
            {
                continue;
            }

            bool valid_sample_world_position = false;
            const float3 sample_world_position = GetWorldPosition(sample_pixel, sample_depth, valid_sample_world_position);
            if (!valid_sample_world_position)
            {
                continue;
            }

            const float4 sample_encoded_normal = normalTex.Load(int3(sample_pixel, 0));
            if (dot(sample_encoded_normal.xyz, sample_encoded_normal.xyz) < 1e-8f)
            {
                continue;
            }

            const float sample_ao = InputAOTex.Load(int3(sample_pixel, 0));
            const float3 sample_normal = DecodeNormal(sample_encoded_normal);

            const float spatial_weight = 1.0f / (1.0f + dot(float2(sample_offset), float2(sample_offset)));
            const float world_distance = length(sample_world_position - center_world_position);
            const float depth_weight = exp(-world_distance * max(blur_depth_reject, 1e-4f));
            const float normal_weight = exp(-(1.0f - saturate(dot(center_normal, sample_normal))) * max(blur_normal_reject, 1e-4f));
            const float sample_weight = spatial_weight * depth_weight * normal_weight;

            ao_sum += sample_ao * sample_weight;
            weight_sum += sample_weight;
        }
    }

    return saturate(ao_sum / max(weight_sum, 1e-4f));
}

[numthreads(8, 8, 1)]
void MainSSAO(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint2 viewport_extent = GetViewportExtent();
    if (dispatch_thread_id.x >= viewport_extent.x || dispatch_thread_id.y >= viewport_extent.y)
    {
        return;
    }

    const uint2 pixel = dispatch_thread_id.xy;
    Output[pixel] = ComputeSSAO(pixel);
}

[numthreads(8, 8, 1)]
void MainSSAOBlur(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint2 viewport_extent = GetViewportExtent();
    if (dispatch_thread_id.x >= viewport_extent.x || dispatch_thread_id.y >= viewport_extent.y)
    {
        return;
    }

    const uint2 pixel = dispatch_thread_id.xy;
    Output[pixel] = ComputeBlurredSSAO(pixel);
}

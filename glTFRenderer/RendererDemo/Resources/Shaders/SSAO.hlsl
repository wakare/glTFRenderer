#include "SceneViewCommon.hlsl"

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
    uint debug_output_mode;
};

static const float kClearDepthEpsilon = 1e-5f;
static const float kTwoPi = 6.28318530718f;
static const uint kSSAOKernelSize = 32u;
static const float3 kSSAOSampleKernel[kSSAOKernelSize] = {
    float3(0.992157f, 0.000000f, 0.125000f),
    float3(-0.719879f, -0.659468f, 0.216506f),
    float3(0.083941f, 0.956467f, 0.279508f),
    float3(0.574202f, -0.748944f, 0.330719f),
    float3(-0.912854f, 0.161471f, 0.375000f),
    float3(0.767829f, 0.488430f, 0.414578f),
    float3(-0.231743f, -0.862073f, 0.450694f),
    float3(-0.403294f, 0.776517f, 0.484123f),
    float3(0.804958f, -0.293969f, 0.515388f),
    float3(-0.775087f, -0.319945f, 0.544862f),
    float3(0.347418f, 0.742412f, 0.572822f),
    float3(0.239544f, -0.763704f, 0.599479f),
    float3(-0.675405f, 0.391411f, 0.625000f),
    float3(0.742611f, 0.163261f, 0.649519f),
    float3(-0.425314f, -0.604965f, 0.673146f),
    float3(-0.092280f, 0.712116f, 0.695971f),
    float3(0.532173f, -0.448516f, 0.718070f),
    float3(-0.672571f, -0.027813f, 0.739510f),
    float3(0.460398f, 0.458158f, 0.760345f),
    float3(-0.028870f, -0.624333f, 0.780625f),
    float3(-0.384092f, 0.460270f, 0.800391f),
    float3(0.567706f, -0.076384f, 0.819680f),
    float3(-0.447255f, -0.311188f, 0.838525f),
    float3(0.113118f, 0.502821f, 0.856957f),
    float3(0.240697f, -0.420048f, 0.875000f),
    float3(-0.429373f, 0.136982f, 0.892679f),
    float3(0.376350f, 0.173883f, 0.910014f),
    float3(-0.144773f, -0.345927f, 0.927025f),
    float3(-0.111933f, 0.311201f, 0.943729f),
    float3(0.247418f, -0.130036f, 0.960143f),
    float3(-0.209355f, -0.055185f, 0.976281f),
    float3(0.067605f, 0.105141f, 0.992157f)
};

uint2 GetViewportExtent()
{
    return uint2(max(viewport_width, 1u), max(viewport_height, 1u));
}

float2 GetViewportSize()
{
    return max(float2((float)viewport_width, (float)viewport_height), float2(1.0f, 1.0f));
}

bool IsDepthValid(float depth)
{
    return depth < (1.0f - kClearDepthEpsilon);
}

float3 DecodeNormal(float4 encoded_normal)
{
    return normalize(encoded_normal.xyz * 2.0f - 1.0f);
}

float2 TexelToUV(uint2 texel)
{
    return (float2(texel) + 0.5f) / GetViewportSize();
}

float3 GetViewPositionFromUV(float2 uv, float depth, out bool valid_view_position)
{
    if (!IsDepthValid(depth))
    {
        valid_view_position = false;
        return 0.0f;
    }

    const float4 clip_position = float4(2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y, depth, 1.0f);
    const float4 view_position_h = mul(inverse_projection_matrix, clip_position);
    if (abs(view_position_h.w) < 1.0e-6f)
    {
        valid_view_position = false;
        return 0.0f;
    }

    valid_view_position = true;
    return view_position_h.xyz / view_position_h.w;
}

float3 GetViewPosition(uint2 texel, float depth, out bool valid_view_position)
{
    return GetViewPositionFromUV(TexelToUV(texel), depth, valid_view_position);
}

float3 WorldNormalToView(float3 world_normal)
{
    return normalize(mul((float3x3)view_matrix, world_normal));
}

bool ProjectViewPositionToUV(float3 view_position, out float2 out_uv)
{
    const float4 clip_position = mul(projection_matrix, float4(view_position, 1.0f));
    if (abs(clip_position.w) < 1.0e-6f)
    {
        out_uv = 0.0f;
        return false;
    }

    const float3 clip_ndc = clip_position.xyz / clip_position.w;
    out_uv = float2(
        clip_ndc.x * 0.5f + 0.5f,
        0.5f - clip_ndc.y * 0.5f);
    return clip_ndc.z >= 0.0f &&
           clip_ndc.z <= 1.0f &&
           all(out_uv >= float2(0.0f, 0.0f)) &&
           all(out_uv <= float2(1.0f, 1.0f));
}

float3 MakeFallbackTangent(float3 normal)
{
    const float3 fallback_axis = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    return normalize(cross(fallback_axis, normal));
}

float InterleavedGradientNoise(uint2 pixel)
{
    return frac(52.9829189f * frac(dot(float2(pixel), float2(0.06711056f, 0.00583715f))));
}

float ComputeSSAO(uint2 pixel)
{
    const bool output_valid_sample_ratio = debug_output_mode == 1u;
    if (enabled == 0u)
    {
        return output_valid_sample_ratio ? 0.0f : 1.0f;
    }

    const float depth = depthTex.Load(int3(pixel, 0));
    if (!IsDepthValid(depth))
    {
        return output_valid_sample_ratio ? 0.0f : 1.0f;
    }

    const float4 encoded_normal = normalTex.Load(int3(pixel, 0));
    if (dot(encoded_normal.xyz, encoded_normal.xyz) < 1e-8f)
    {
        return output_valid_sample_ratio ? 0.0f : 1.0f;
    }

    bool valid_view_position = false;
    const float3 view_position = GetViewPosition(pixel, depth, valid_view_position);
    if (!valid_view_position)
    {
        return output_valid_sample_ratio ? 0.0f : 1.0f;
    }

    const float3 view_normal = WorldNormalToView(DecodeNormal(encoded_normal));
    const uint2 viewport_extent = GetViewportExtent();
    const float3 tangent_base = MakeFallbackTangent(view_normal);
    const float3 bitangent_base = normalize(cross(view_normal, tangent_base));
    const float sample_rotation = kTwoPi * InterleavedGradientNoise(pixel);
    float sample_rotation_sin = 0.0f;
    float sample_rotation_cos = 0.0f;
    sincos(sample_rotation, sample_rotation_sin, sample_rotation_cos);
    const float3 tangent = tangent_base * sample_rotation_cos + bitangent_base * sample_rotation_sin;
    const float3 bitangent = bitangent_base * sample_rotation_cos - tangent_base * sample_rotation_sin;

    const uint clamped_sample_count = clamp(sample_count, 1u, 32u);
    const float clamped_radius = max(radius_world, 1e-4f);
    const float clamped_bias = max(bias, 1e-5f);
    const float clamped_thickness = max(thickness, 1e-4f);
    const float clamped_distribution_power = max(sample_distribution_power, 0.25f);
    const float inv_sample_count = rcp(float(clamped_sample_count));

    float occlusion_sum = 0.0f;
    uint valid_sample_count = 0u;

    [loop]
    for (uint sample_index = 0u; sample_index < clamped_sample_count; ++sample_index)
    {
        const float sample_ratio = (float(sample_index) + 0.5f) * inv_sample_count;
        const float sample_scale = lerp(0.10f, 1.0f, pow(sample_ratio, clamped_distribution_power));
        const float sample_distance = clamped_radius * sample_scale;
        const uint kernel_index = min((sample_index * kSSAOKernelSize) / clamped_sample_count, kSSAOKernelSize - 1u);
        const float3 hemisphere = kSSAOSampleKernel[kernel_index];
        const float3 sample_direction = tangent * hemisphere.x + bitangent * hemisphere.y + view_normal * hemisphere.z;

        float2 sample_uv = 0.0f;
        if (!ProjectViewPositionToUV(view_position + sample_direction * sample_distance, sample_uv))
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

        bool valid_sample_view_position = false;
        const float3 sample_view_position = GetViewPosition(sample_pixel, sample_depth, valid_sample_view_position);
        if (!valid_sample_view_position)
        {
            continue;
        }

        const float3 sample_delta = sample_view_position - view_position;
        const float along_ray = dot(sample_delta, sample_direction);
        const float along_normal = dot(sample_delta, view_normal);
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
        const float distance_weight = 1.0f - sample_scale;
        const float thickness_weight = smoothstep(0.0f, clamped_thickness, occlusion_gap);
        const float angle_weight = saturate(dot(normalize(sample_delta), view_normal));
        occlusion_sum += distance_weight * thickness_weight * angle_weight;
    }

    if (output_valid_sample_ratio)
    {
        return saturate(float(valid_sample_count) * inv_sample_count);
    }

    if (valid_sample_count == 0u)
    {
        return 1.0f;
    }

    const float normalized_occlusion = saturate((occlusion_sum / float(valid_sample_count)) * intensity);
    return saturate(pow(1.0f - normalized_occlusion, max(power, 1e-4f)));
}

float ComputeBlurredSSAO(uint2 pixel, int2 axis)
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

    bool valid_center_view_position = false;
    const float3 center_view_position = GetViewPosition(pixel, center_depth, valid_center_view_position);
    if (!valid_center_view_position)
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
    for (int offset = -clamped_blur_radius; offset <= clamped_blur_radius; ++offset)
    {
        if (offset == 0)
        {
            continue;
        }

        const int2 sample_offset = axis * offset;
        const uint2 sample_pixel = uint2(clamp(
            int2(pixel) + sample_offset,
            int2(0, 0),
            int2(viewport_extent) - 1));

        const float sample_depth = depthTex.Load(int3(sample_pixel, 0));
        if (!IsDepthValid(sample_depth))
        {
            continue;
        }

        bool valid_sample_view_position = false;
        const float3 sample_view_position = GetViewPosition(sample_pixel, sample_depth, valid_sample_view_position);
        if (!valid_sample_view_position)
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
        const float view_distance = length(sample_view_position - center_view_position);
        const float depth_weight = exp(-view_distance * max(blur_depth_reject, 1e-4f));
        const float normal_weight = exp(-(1.0f - saturate(dot(center_normal, sample_normal))) * max(blur_normal_reject, 1e-4f));
        const float sample_weight = spatial_weight * depth_weight * normal_weight;

        ao_sum += sample_ao * sample_weight;
        weight_sum += sample_weight;
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
void MainSSAOBlurHorizontal(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint2 viewport_extent = GetViewportExtent();
    if (dispatch_thread_id.x >= viewport_extent.x || dispatch_thread_id.y >= viewport_extent.y)
    {
        return;
    }

    const uint2 pixel = dispatch_thread_id.xy;
    Output[pixel] = ComputeBlurredSSAO(pixel, int2(1, 0));
}

[numthreads(8, 8, 1)]
void MainSSAOBlurVertical(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint2 viewport_extent = GetViewportExtent();
    if (dispatch_thread_id.x >= viewport_extent.x || dispatch_thread_id.y >= viewport_extent.y)
    {
        return;
    }

    const uint2 pixel = dispatch_thread_id.xy;
    Output[pixel] = ComputeBlurredSSAO(pixel, int2(0, 1));
}

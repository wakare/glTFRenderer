#include "SceneViewCommon.hlsl"

Texture2D<float4> InputColorTex;
Texture2D<float> InputDepthTex;
Texture2D<float4> InputNormalTex;
Texture2D<float4> BlurredColorTex;
Texture2D<float4> QuarterBlurredColorTex;
RWTexture2D<float4> Output;

struct FrostedGlassPanelData
{
    float4 center_half_size;    // xy: center uv, zw: half size uv
    float4 corner_blur_rim;     // x: corner radius, y: blur sigma, z: blur strength, w: rim intensity
    float4 tint_depth_weight;   // rgb: tint color, w: depth-aware weight scale
    float4 shape_info;          // x: shape type, y: edge softness, z: custom shape index, w: reserved
    float4 optical_info;        // x: thickness, y: refraction strength, z: fresnel intensity, w: fresnel power
};

cbuffer FrostedGlassGlobalBuffer
{
    uint panel_count;
    uint blur_radius;
    float scene_edge_scale;
    float pad0;
};
StructuredBuffer<FrostedGlassPanelData> g_frosted_panels;

static const uint PANEL_SHAPE_ROUNDED_RECT = 0;
static const uint PANEL_SHAPE_CIRCLE = 1;
static const uint PANEL_SHAPE_MASK = 2;

int2 ClampToViewport(int2 pixel)
{
    return clamp(pixel, int2(0, 0), int2((int)viewport_width - 1, (int)viewport_height - 1));
}

float SampleDepthSafe(int2 pixel)
{
    return InputDepthTex.Load(int3(ClampToViewport(pixel), 0)).r;
}

float3 SampleNormalSafe(int2 pixel)
{
    float3 normal = InputNormalTex.Load(int3(ClampToViewport(pixel), 0)).xyz * 2.0f - 1.0f;
    const float length_sq = dot(normal, normal);
    if (length_sq < 1e-6f)
    {
        return float3(0.0f, 0.0f, 1.0f);
    }
    return normal * rsqrt(length_sq);
}

bool IsDepthValid(float depth)
{
    const float clear_depth_epsilon = 1e-5f;
    return depth < (1.0f - clear_depth_epsilon);
}

bool TryGetWorldPosition(int2 pixel, float depth, out float3 world_position)
{
    if (!IsDepthValid(depth))
    {
        world_position = 0.0f;
        return false;
    }

    const float2 uv = pixel / float2(viewport_width - 1, viewport_height - 1);
    const float4 clip_space_coord = float4(2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y, depth, 1.0f);
    float4 world_space_coord = mul(inverse_view_projection_matrix, clip_space_coord);

    if (abs(world_space_coord.w) < 1e-6f)
    {
        world_position = 0.0f;
        return false;
    }

    world_position = world_space_coord.xyz / world_space_coord.w;
    return true;
}

float RoundedRectSDF(float2 p, float2 center, float2 half_size, float corner_radius)
{
    float2 q = abs(p - center) - half_size + corner_radius;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - corner_radius;
}

float EllipseSDF(float2 p, float2 center, float2 half_size)
{
    const float2 safe_half_size = max(half_size, float2(1e-5f, 1e-5f));
    const float2 q = (p - center) / safe_half_size;
    return (length(q) - 1.0f) * min(safe_half_size.x, safe_half_size.y);
}

float CalcPanelSDF(float2 uv, FrostedGlassPanelData panel_data)
{
    const float shape_type = panel_data.shape_info.x;
    const float2 panel_center = panel_data.center_half_size.xy;
    const float2 panel_half_size = panel_data.center_half_size.zw;
    const float panel_corner_radius = panel_data.corner_blur_rim.x;
    if (shape_type < ((float)PANEL_SHAPE_CIRCLE - 0.5f))
    {
        return RoundedRectSDF(uv, panel_center, panel_half_size, panel_corner_radius);
    }
    if (shape_type < ((float)PANEL_SHAPE_MASK - 0.5f))
    {
        return EllipseSDF(uv, panel_center, panel_half_size);
    }

    // Reserved for mask/irregular shape path. Fallback to rounded rect for now.
    return RoundedRectSDF(uv, panel_center, panel_half_size, panel_corner_radius);
}

float CalcPanelMaskFromSDF(float sdf, float edge_softness)
{
    const float edge_scale = max(edge_softness, 0.1f);
    const float aa = edge_scale * 2.0f / min((float)viewport_width, (float)viewport_height);
    return 1.0f - smoothstep(0.0f, aa, sdf);
}

float CalcPanelRimFromSDF(float sdf, float edge_softness)
{
    const float edge_scale = max(edge_softness, 0.1f);
    const float aa = edge_scale * 2.0f / min((float)viewport_width, (float)viewport_height);
    return exp(-abs(sdf) / (aa * 4.0f));
}

float2 CalcPanelNormalFromSDF(float2 uv, FrostedGlassPanelData panel_data)
{
    const float pixel_to_uv = 1.0f / min((float)viewport_width, (float)viewport_height);
    const float2 du = float2(pixel_to_uv, 0.0f);
    const float2 dv = float2(0.0f, pixel_to_uv);
    const float dsdx = CalcPanelSDF(uv + du, panel_data) - CalcPanelSDF(uv - du, panel_data);
    const float dsdy = CalcPanelSDF(uv + dv, panel_data) - CalcPanelSDF(uv - dv, panel_data);
    const float2 gradient = float2(dsdx, dsdy);
    const float gradient_len_sq = dot(gradient, gradient);
    if (gradient_len_sq < 1e-8f)
    {
        return float2(0.0f, 0.0f);
    }
    return gradient * rsqrt(gradient_len_sq);
}

float3 SampleBlurredColor(float2 uv)
{
    uint blurred_width = 0;
    uint blurred_height = 0;
    BlurredColorTex.GetDimensions(blurred_width, blurred_height);
    const int2 max_pixel = int2((int)max(blurred_width, 1u) - 1, (int)max(blurred_height, 1u) - 1);
    int2 blurred_pixel = int2(uv * float2((float)max(blurred_width, 1u), (float)max(blurred_height, 1u)));
    blurred_pixel = clamp(blurred_pixel, int2(0, 0), max_pixel);
    return BlurredColorTex.Load(int3(blurred_pixel, 0)).rgb;
}

float3 SampleQuarterBlurredColor(float2 uv)
{
    uint blurred_width = 0;
    uint blurred_height = 0;
    QuarterBlurredColorTex.GetDimensions(blurred_width, blurred_height);
    const int2 max_pixel = int2((int)max(blurred_width, 1u) - 1, (int)max(blurred_height, 1u) - 1);
    int2 blurred_pixel = int2(uv * float2((float)max(blurred_width, 1u), (float)max(blurred_height, 1u)));
    blurred_pixel = clamp(blurred_pixel, int2(0, 0), max_pixel);
    return QuarterBlurredColorTex.Load(int3(blurred_pixel, 0)).rgb;
}

[numthreads(8, 8, 1)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= viewport_width || dispatch_thread_id.y >= viewport_height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float3 scene_color = InputColorTex.Load(int3(pixel, 0)).rgb;
    const float2 uv = (float2(pixel) + 0.5f) / float2((float)viewport_width, (float)viewport_height);
    float3 final_color = scene_color;
    if (panel_count == 0)
    {
        Output[pixel] = float4(final_color, 1.0f);
        return;
    }

    const float depth_dx = abs(SampleDepthSafe(pixel + int2(1, 0)) - SampleDepthSafe(pixel - int2(1, 0)));
    const float depth_dy = abs(SampleDepthSafe(pixel + int2(0, 1)) - SampleDepthSafe(pixel - int2(0, 1)));
    const float scene_edge = saturate((depth_dx + depth_dy) * scene_edge_scale);
    const float center_depth = SampleDepthSafe(pixel);
    const float3 center_normal = SampleNormalSafe(pixel);
    float3 world_position = 0.0f;
    const bool valid_world_position = TryGetWorldPosition(pixel, center_depth, world_position);
    const float3 view_dir = valid_world_position ? normalize(view_position.xyz - world_position) : float3(0.0f, 0.0f, 1.0f);
    const float n_dot_v = saturate(abs(dot(center_normal, view_dir)));

    [loop]
    for (uint panel_index = 0; panel_index < panel_count; ++panel_index)
    {
        const FrostedGlassPanelData panel_data = g_frosted_panels[panel_index];
        const float panel_blur_sigma = panel_data.corner_blur_rim.y;
        const float panel_blur_strength = panel_data.corner_blur_rim.z;
        const float panel_rim_intensity = panel_data.corner_blur_rim.w;
        const float3 panel_tint = panel_data.tint_depth_weight.rgb;
        const float panel_depth_weight_scale = panel_data.tint_depth_weight.w;
        const float panel_edge_softness = panel_data.shape_info.y;
        const float panel_thickness = panel_data.optical_info.x;
        const float panel_refraction_strength = panel_data.optical_info.y;
        const float panel_fresnel_intensity = panel_data.optical_info.z;
        const float panel_fresnel_power = panel_data.optical_info.w;

        const float panel_sdf = CalcPanelSDF(uv, panel_data);
        const float panel_mask = CalcPanelMaskFromSDF(panel_sdf, panel_edge_softness);
        if (panel_mask <= 1e-4f)
        {
            continue;
        }

        const float2 panel_normal_uv = CalcPanelNormalFromSDF(uv, panel_data);
        float2 refraction_direction = panel_normal_uv * 0.7f + center_normal.xy * 0.3f;
        const float refraction_dir_len_sq = dot(refraction_direction, refraction_direction);
        if (refraction_dir_len_sq > 1e-6f)
        {
            refraction_direction *= rsqrt(refraction_dir_len_sq);
        }
        else
        {
            refraction_direction = 0.0f;
        }

        const float rim = CalcPanelRimFromSDF(panel_sdf, panel_edge_softness);
        const float fresnel_term = pow(saturate(1.0f - n_dot_v), max(panel_fresnel_power, 1.0f));
        const float mixed_fresnel = saturate(fresnel_term + rim * 0.35f);
        const float2 refraction_uv = uv + refraction_direction * panel_thickness * panel_refraction_strength * (0.2f + mixed_fresnel);
        const int2 refracted_pixel = ClampToViewport(int2(refraction_uv * float2((float)viewport_width, (float)viewport_height)));
        float refracted_depth = SampleDepthSafe(refracted_pixel);
        if (!IsDepthValid(refracted_depth))
        {
            refracted_depth = center_depth;
        }
        const float depth_delta = abs(refracted_depth - center_depth);
        const float depth_aware_weight = exp(-depth_delta * panel_depth_weight_scale);
        const float effective_blur_strength = panel_blur_strength * saturate(0.25f + 0.75f * depth_aware_weight);
        const float blur_sigma_factor = saturate(panel_blur_sigma / 8.0f);
        const float3 half_blurred_color = SampleBlurredColor(refraction_uv);
        const float3 quarter_blurred_color = SampleQuarterBlurredColor(refraction_uv);
        const float quarter_blend = saturate((panel_blur_sigma - 4.0f) / 6.0f);
        const float3 blurred_color = lerp(half_blurred_color, quarter_blurred_color, quarter_blend);
        const float3 sigma_adjusted_blur = lerp(final_color, blurred_color, blur_sigma_factor);
        float3 frosted_color = lerp(final_color, sigma_adjusted_blur * panel_tint, effective_blur_strength);

        const float scene_luminance = dot(final_color, float3(0.2126f, 0.7152f, 0.0722f));
        const float highlight_compress = lerp(1.0f, 0.45f, saturate(scene_luminance));
        const float rim_term = rim * panel_rim_intensity * (1.0f + scene_edge * 1.5f);
        const float fresnel_highlight = mixed_fresnel * panel_fresnel_intensity * highlight_compress;
        const float3 highlight_tint = lerp(float3(1.0f, 1.0f, 1.0f), panel_tint, 0.35f);
        frosted_color += highlight_tint * (rim_term + fresnel_highlight);

        final_color = lerp(final_color, frosted_color, panel_mask);
    }

    Output[pixel] = float4(final_color, 1.0f);
}

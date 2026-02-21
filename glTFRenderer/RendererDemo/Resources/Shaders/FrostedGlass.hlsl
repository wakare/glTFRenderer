#include "SceneViewCommon.hlsl"

Texture2D<float4> InputColorTex;
Texture2D<float> InputDepthTex;
RWTexture2D<float4> Output;

struct FrostedGlassPanelData
{
    float4 center_half_size;    // xy: center uv, zw: half size uv
    float4 corner_blur_rim;     // x: corner radius, y: blur sigma, z: blur strength, w: rim intensity
    float4 tint_depth_weight;   // rgb: tint color, w: depth-aware weight scale
    float4 shape_info;          // x: shape type, y: edge softness, z: custom shape index, w: reserved
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

float3 SampleColorSafe(int2 pixel)
{
    return InputColorTex.Load(int3(ClampToViewport(pixel), 0)).rgb;
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

float3 CalcDepthAwareBlurColor(int2 center_pixel, float center_depth, float blur_sigma, float depth_weight_scale)
{
    float3 accumulated_color = 0.0f;
    float accumulated_weight = 0.0f;

    const int radius = int(blur_radius);
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            int2 sample_pixel = center_pixel + int2(x, y);
            const float2 offset = float2((float)x, (float)y);
            const float spatial_weight = exp(-dot(offset, offset) / max(2.0f * blur_sigma * blur_sigma, 1e-5f));

            const float sample_depth = SampleDepthSafe(sample_pixel);
            const float depth_weight = exp(-abs(sample_depth - center_depth) * depth_weight_scale);

            const float weight = spatial_weight * depth_weight;
            accumulated_color += SampleColorSafe(sample_pixel) * weight;
            accumulated_weight += weight;
        }
    }

    return accumulated_color / max(accumulated_weight, 1e-5f);
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

        const float panel_sdf = CalcPanelSDF(uv, panel_data);
        const float panel_mask = CalcPanelMaskFromSDF(panel_sdf, panel_edge_softness);
        if (panel_mask <= 1e-4f)
        {
            continue;
        }

        const float3 blurred_color = CalcDepthAwareBlurColor(pixel, center_depth, panel_blur_sigma, panel_depth_weight_scale);
        float3 frosted_color = lerp(final_color, blurred_color * panel_tint, panel_blur_strength);
        const float rim = CalcPanelRimFromSDF(panel_sdf, panel_edge_softness);
        frosted_color += rim * (panel_rim_intensity + scene_edge * panel_rim_intensity * 2.5f);
        final_color = lerp(final_color, frosted_color, panel_mask);
    }

    Output[pixel] = float4(final_color, 1.0f);
}

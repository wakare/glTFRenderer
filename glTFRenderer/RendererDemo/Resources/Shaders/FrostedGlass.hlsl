#include "SceneViewCommon.hlsl"

Texture2D<float4> InputColorTex;
Texture2D<float> InputDepthTex;
Texture2D<float4> InputNormalTex;
Texture2D<float4> BlurredColorTex;
Texture2D<float4> QuarterBlurredColorTex;
Texture2D<float4> EighthBlurredColorTex;
Texture2D<float4> SixteenthBlurredColorTex;
Texture2D<float4> ThirtySecondBlurredColorTex;
Texture2D<float4> BackCompositeTex;
Texture2D<float4> MaskParamTex;
Texture2D<float4> MaskParamSecondaryTex;
Texture2D<float4> PanelOpticsTex;
Texture2D<float4> PanelOpticsSecondaryTex;
Texture2D<float4> PanelProfileTex;
Texture2D<float4> PanelProfileSecondaryTex;
Texture2D<float4> FrontMaskParamTex;
Texture2D<float4> HistoryInputTex;
Texture2D<float4> VelocityTex;
RWTexture2D<float4> MaskParamOutput;
RWTexture2D<float4> MaskParamSecondaryOutput;
RWTexture2D<float4> PanelOpticsOutput;
RWTexture2D<float4> PanelOpticsSecondaryOutput;
RWTexture2D<float4> PanelProfileOutput;
RWTexture2D<float4> PanelProfileSecondaryOutput;
RWTexture2D<float4> Output;
RWTexture2D<float4> BackCompositeOutput;
RWTexture2D<float4> HistoryOutputTex;

struct FrostedGlassPanelData
{
    float4 center_half_size;    // xy: center uv, zw: half size uv
    float4 corner_blur_rim;     // x: corner radius, y: blur sigma, z: blur strength, w: rim intensity
    float4 tint_depth_weight;   // rgb: tint color, w: depth-aware weight scale
    float4 shape_info;          // x: shape type, y: edge softness, z: custom shape index, w: panel alpha
    float4 optical_info;        // x: thickness, y: refraction strength, z: fresnel intensity, w: fresnel power
    float4 layering_info;       // x: layer order
};

cbuffer FrostedGlassGlobalBuffer
{
    uint panel_count;
    uint blur_radius;
    float scene_edge_scale;
    float blur_kernel_sigma_scale;
    float temporal_history_blend;
    float temporal_reject_velocity;
    float temporal_edge_reject;
    uint temporal_history_valid;
    uint multilayer_mode;
    float multilayer_overlap_threshold;
    float multilayer_back_layer_weight;
    float multilayer_front_transmittance;
    uint multilayer_runtime_enabled;
    float multilayer_frame_budget_ms;
    float blur_quarter_mix_boost;
    float blur_response_scale;
    float blur_sigma_normalization;
    float depth_aware_min_strength;
    float blur_veil_strength;
    float blur_contrast_compression;
    float blur_veil_tint_mix;
    float blur_detail_preservation;
    float pad0;
};
StructuredBuffer<FrostedGlassPanelData> g_frosted_panels;

static const uint PANEL_SHAPE_ROUNDED_RECT = 0;
static const uint PANEL_SHAPE_CIRCLE = 1;
static const uint PANEL_SHAPE_MASK = 2;
static const uint MULTILAYER_MODE_SINGLE = 0;
static const uint MULTILAYER_MODE_AUTO = 1;
static const uint MULTILAYER_MODE_FORCE = 2;

bool ShouldEnableMultilayer(bool has_front_layer, bool has_back_layer, float back_mask)
{
    if (multilayer_runtime_enabled == 0)
    {
        return false;
    }

    if (multilayer_mode == MULTILAYER_MODE_FORCE)
    {
        return has_front_layer && has_back_layer;
    }

    if (multilayer_mode == MULTILAYER_MODE_AUTO)
    {
        return has_front_layer &&
               has_back_layer &&
               back_mask > multilayer_overlap_threshold;
    }

    return false;
}

float ComputeEffectiveBackMask(float back_mask, float front_mask)
{
    const float front_transmittance =
        lerp(1.0f, saturate(multilayer_front_transmittance), saturate(front_mask));
    return saturate(back_mask * multilayer_back_layer_weight * front_transmittance);
}

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

float DiamondSDF(float2 p)
{
    return abs(p.x) + abs(p.y) - 1.0f;
}

float CrossSDF(float2 p, float arm_thickness)
{
    const float2 abs_p = abs(p);
    const float horizontal = max(abs_p.x - 1.0f, abs_p.y - arm_thickness);
    const float vertical = max(abs_p.x - arm_thickness, abs_p.y - 1.0f);
    return min(horizontal, vertical);
}

float LobeShapeSDF(float2 p, float shape_seed)
{
    const float radius = length(p);
    const float angle = atan2(p.y, p.x);
    const float base_lobes = 3.0f + fmod(abs(floor(shape_seed + 0.5f)), 5.0f);
    const float phase = shape_seed * 0.73f;
    const float target_radius =
        1.0f +
        0.16f * sin(angle * base_lobes + phase) +
        0.08f * sin(angle * (base_lobes * 2.0f + 1.0f) - phase * 0.5f);
    return radius - target_radius;
}

float ShapeMaskSDF(float2 uv, float2 center, float2 half_size, float custom_shape_index)
{
    const float2 safe_half_size = max(half_size, float2(1e-5f, 1e-5f));
    const float2 local_p = (uv - center) / safe_half_size;
    const float shape_scale = min(safe_half_size.x, safe_half_size.y);

    const float rounded_shape_index = floor(abs(custom_shape_index) + 0.5f);
    const float shape_variant = fmod(rounded_shape_index, 4.0f);

    float sdf_normalized = 0.0f;
    if (shape_variant < 0.5f)
    {
        sdf_normalized = LobeShapeSDF(local_p, custom_shape_index);
    }
    else if (shape_variant < 1.5f)
    {
        sdf_normalized = DiamondSDF(local_p);
    }
    else if (shape_variant < 2.5f)
    {
        sdf_normalized = CrossSDF(local_p, 0.40f);
    }
    else
    {
        const float lobe = LobeShapeSDF(local_p, custom_shape_index + 1.0f);
        const float diamond = DiamondSDF(local_p * float2(1.08f, 0.94f));
        sdf_normalized = lerp(lobe, diamond, 0.55f);
    }

    return sdf_normalized * shape_scale;
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

    return ShapeMaskSDF(uv, panel_center, panel_half_size, panel_data.shape_info.z);
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

float3 SampleEighthBlurredColor(float2 uv)
{
    uint blurred_width = 0;
    uint blurred_height = 0;
    EighthBlurredColorTex.GetDimensions(blurred_width, blurred_height);
    const int2 max_pixel = int2((int)max(blurred_width, 1u) - 1, (int)max(blurred_height, 1u) - 1);
    int2 blurred_pixel = int2(uv * float2((float)max(blurred_width, 1u), (float)max(blurred_height, 1u)));
    blurred_pixel = clamp(blurred_pixel, int2(0, 0), max_pixel);
    return EighthBlurredColorTex.Load(int3(blurred_pixel, 0)).rgb;
}

float3 SampleSixteenthBlurredColor(float2 uv)
{
    uint blurred_width = 0;
    uint blurred_height = 0;
    SixteenthBlurredColorTex.GetDimensions(blurred_width, blurred_height);
    const int2 max_pixel = int2((int)max(blurred_width, 1u) - 1, (int)max(blurred_height, 1u) - 1);
    int2 blurred_pixel = int2(uv * float2((float)max(blurred_width, 1u), (float)max(blurred_height, 1u)));
    blurred_pixel = clamp(blurred_pixel, int2(0, 0), max_pixel);
    return SixteenthBlurredColorTex.Load(int3(blurred_pixel, 0)).rgb;
}

float3 SampleThirtySecondBlurredColor(float2 uv)
{
    uint blurred_width = 0;
    uint blurred_height = 0;
    ThirtySecondBlurredColorTex.GetDimensions(blurred_width, blurred_height);
    const int2 max_pixel = int2((int)max(blurred_width, 1u) - 1, (int)max(blurred_height, 1u) - 1);
    int2 blurred_pixel = int2(uv * float2((float)max(blurred_width, 1u), (float)max(blurred_height, 1u)));
    blurred_pixel = clamp(blurred_pixel, int2(0, 0), max_pixel);
    return ThirtySecondBlurredColorTex.Load(int3(blurred_pixel, 0)).rgb;
}

bool IsBetterPanelCandidate(float candidate_layer,
                            float candidate_mask,
                            int candidate_panel_index,
                            float selected_layer,
                            float selected_mask,
                            int selected_panel_index)
{
    const float layer_delta = candidate_layer - selected_layer;
    const bool higher_layer = layer_delta > 1e-4f;
    const bool same_layer = abs(layer_delta) <= 1e-4f;
    const bool higher_mask = candidate_mask > selected_mask + 1e-4f;
    const bool same_mask = abs(candidate_mask - selected_mask) <= 1e-4f;
    const bool newer_panel_index = candidate_panel_index > selected_panel_index;
    return higher_layer || (same_layer && (higher_mask || (same_mask && newer_panel_index)));
}

void EvaluatePanelCandidatePayload(float2 uv,
                                   float center_depth,
                                   float3 center_normal,
                                   float n_dot_v,
                                   FrostedGlassPanelData panel_data,
                                   out float panel_mask,
                                   out float panel_rim,
                                   out float panel_mixed_fresnel,
                                   out float2 panel_refraction_uv,
                                   out float panel_effective_blur_strength,
                                   out float2 panel_profile_normal_uv,
                                   out float panel_optical_thickness)
{
    panel_mask = 0.0f;
    panel_rim = 0.0f;
    panel_mixed_fresnel = 0.0f;
    panel_refraction_uv = uv;
    panel_effective_blur_strength = 0.0f;
    panel_profile_normal_uv = float2(0.0f, 0.0f);
    panel_optical_thickness = 0.0f;

    const float panel_edge_softness = panel_data.shape_info.y;
    const float panel_sdf = CalcPanelSDF(uv, panel_data);
    const float panel_alpha = saturate(panel_data.shape_info.w);
    panel_mask = CalcPanelMaskFromSDF(panel_sdf, panel_edge_softness) * panel_alpha;
    if (panel_mask <= 1e-4f)
    {
        return;
    }

    const float panel_depth_weight_scale = panel_data.tint_depth_weight.w;
    const float panel_blur_strength = panel_data.corner_blur_rim.z;
    const float panel_thickness = panel_data.optical_info.x;
    const float panel_refraction_strength = panel_data.optical_info.y;
    const float panel_fresnel_power = panel_data.optical_info.w;

    const float2 panel_normal_uv = CalcPanelNormalFromSDF(uv, panel_data);
    panel_profile_normal_uv = panel_normal_uv;
    panel_optical_thickness = panel_thickness;
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

    panel_rim = CalcPanelRimFromSDF(panel_sdf, panel_edge_softness);
    const float fresnel_term = pow(saturate(1.0f - n_dot_v), max(panel_fresnel_power, 1.0f));
    panel_mixed_fresnel = saturate(fresnel_term + panel_rim * 0.35f);
    const float2 refraction_uv =
        uv + refraction_direction * panel_thickness * panel_refraction_strength * (0.2f + panel_mixed_fresnel);
    const int2 refracted_pixel = ClampToViewport(int2(refraction_uv * float2((float)viewport_width, (float)viewport_height)));
    float refracted_depth = SampleDepthSafe(refracted_pixel);
    if (!IsDepthValid(refracted_depth))
    {
        refracted_depth = center_depth;
    }

    const float depth_delta = abs(refracted_depth - center_depth);
    const float depth_aware_weight = exp(-depth_delta * panel_depth_weight_scale);
    const float min_depth_aware_strength = saturate(depth_aware_min_strength);
    panel_effective_blur_strength = panel_blur_strength * lerp(min_depth_aware_strength, 1.0f, depth_aware_weight);
    panel_refraction_uv = saturate(refraction_uv);
    panel_effective_blur_strength = saturate(panel_effective_blur_strength);
}

bool EvaluatePanelFrostedColor(float3 scene_color,
                               float4 mask_param,
                               float4 panel_optics,
                               out float layer_mask,
                               out float3 out_frosted_color,
                               out float out_blur_metric)
{
    layer_mask = panel_count > 0 ? saturate(mask_param.x) : 0.0f;
    out_frosted_color = scene_color;
    out_blur_metric = 0.0f;
    if (layer_mask <= 1e-4f)
    {
        return false;
    }

    const int panel_index = (int)round(mask_param.w);
    if (panel_index < 0 || panel_index >= (int)panel_count)
    {
        layer_mask = 0.0f;
        out_blur_metric = 0.0f;
        return false;
    }

    const FrostedGlassPanelData panel_data = g_frosted_panels[panel_index];
    const float panel_blur_sigma = panel_data.corner_blur_rim.y;
    const float panel_rim_intensity = panel_data.corner_blur_rim.w;
    const float3 panel_tint = panel_data.tint_depth_weight.rgb;
    const float panel_fresnel_intensity = panel_data.optical_info.z;
    const float scene_edge = saturate(panel_optics.w);

    const float2 refraction_uv = saturate(panel_optics.xy);
    const float effective_blur_strength = saturate(panel_optics.z);
    const float blur_response = max(blur_response_scale, 0.1f);
    const float normalized_sigma = panel_blur_sigma * blur_response;
    const float sigma_normalization = max(blur_sigma_normalization, 1.0f);
    const float blur_sigma_factor = saturate(normalized_sigma / sigma_normalization);
    out_blur_metric = saturate(blur_sigma_factor * effective_blur_strength);
    const float3 half_blurred_color = SampleBlurredColor(refraction_uv);
    const float3 quarter_blurred_color = SampleQuarterBlurredColor(refraction_uv);
    const float3 eighth_blurred_color = SampleEighthBlurredColor(refraction_uv);
    const float3 sixteenth_blurred_color = SampleSixteenthBlurredColor(refraction_uv);
    const float3 thirtysecond_blurred_color = SampleThirtySecondBlurredColor(refraction_uv);
    const float quarter_blend = saturate((normalized_sigma - 3.0f) / 5.0f);
    const float eighth_blend = saturate((normalized_sigma - 8.0f) / 8.0f);
    const float sixteenth_blend = saturate((normalized_sigma - 16.0f) / 10.0f);
    const float thirtysecond_blend = saturate((normalized_sigma - 28.0f) / 12.0f);
    const float quarter_mix = saturate(quarter_blend + saturate(blur_quarter_mix_boost) * (1.0f - quarter_blend));
    const float3 half_quarter_blended = lerp(half_blurred_color, quarter_blurred_color, quarter_mix);
    const float3 half_quarter_eighth_blended = lerp(half_quarter_blended, eighth_blurred_color, eighth_blend);
    const float3 half_to_sixteenth_blended = lerp(half_quarter_eighth_blended, sixteenth_blurred_color, sixteenth_blend);
    const float3 layered_blurred_color = lerp(half_to_sixteenth_blended, thirtysecond_blurred_color, thirtysecond_blend);
    const float detail_keep = saturate(blur_detail_preservation);
    const float coarse_bias = 1.0f - detail_keep;
    const float coarse_mix = saturate((normalized_sigma - 10.0f) / 10.0f) * coarse_bias;
    const float3 ultra_low_freq = lerp(sixteenth_blurred_color, thirtysecond_blurred_color, saturate((normalized_sigma - 22.0f) / 12.0f));
    const float3 blurred_color = lerp(layered_blurred_color, ultra_low_freq, coarse_mix);
    const float veil_factor = saturate(blur_veil_strength * blur_sigma_factor * effective_blur_strength);
    float3 veiled_blur = lerp(blurred_color, ultra_low_freq, veil_factor);
    const float veiled_luma = dot(veiled_blur, float3(0.2126f, 0.7152f, 0.0722f));
    const float contrast_keep = 1.0f - saturate(blur_contrast_compression * veil_factor);
    veiled_blur = lerp(float3(veiled_luma, veiled_luma, veiled_luma), veiled_blur, contrast_keep);
    const float tint_neutralize = saturate(blur_veil_tint_mix * veil_factor);
    const float3 veiled_tint = lerp(panel_tint, float3(1.0f, 1.0f, 1.0f), tint_neutralize);
    const float3 sigma_adjusted_blur = lerp(scene_color, veiled_blur, blur_sigma_factor);
    float3 frosted_color = lerp(scene_color, sigma_adjusted_blur * veiled_tint, effective_blur_strength);

    const float rim = saturate(mask_param.y);
    const float mixed_fresnel = saturate(mask_param.z);
    const float scene_luminance = dot(scene_color, float3(0.2126f, 0.7152f, 0.0722f));
    const float highlight_compress = lerp(1.0f, 0.45f, saturate(scene_luminance));
    const float rim_term = rim * panel_rim_intensity * (1.0f + scene_edge * 1.5f);
    const float fresnel_highlight = mixed_fresnel * panel_fresnel_intensity * highlight_compress;
    const float3 highlight_tint = lerp(float3(1.0f, 1.0f, 1.0f), panel_tint, 0.35f);
    frosted_color += highlight_tint * (rim_term + fresnel_highlight);
    out_frosted_color = frosted_color;
    return true;
}

bool TryResolvePanelPayload(float4 mask_param, out float layer_mask)
{
    layer_mask = panel_count > 0 ? saturate(mask_param.x) : 0.0f;
    const int panel_index = (int)round(mask_param.w);
    if (layer_mask <= 1e-4f)
    {
        layer_mask = 0.0f;
        return false;
    }

    if (panel_index < 0 || panel_index >= (int)panel_count)
    {
        layer_mask = 0.0f;
        return false;
    }

    return true;
}

struct PanelPayloadResult
{
    float4 mask_param_front;
    float4 mask_param_back;
    float4 panel_optics_front;
    float4 panel_optics_back;
    float4 panel_profile_front;
    float4 panel_profile_back;
};

PanelPayloadResult EvaluatePanelPayloadAtPixel(int2 pixel, float2 uv)
{
    PanelPayloadResult payload = (PanelPayloadResult)0;
    const float depth_dx = abs(SampleDepthSafe(pixel + int2(1, 0)) - SampleDepthSafe(pixel - int2(1, 0)));
    const float depth_dy = abs(SampleDepthSafe(pixel + int2(0, 1)) - SampleDepthSafe(pixel - int2(0, 1)));
    const float scene_edge = saturate((depth_dx + depth_dy) * scene_edge_scale);

    if (panel_count == 0)
    {
        payload.mask_param_front = float4(0.0f, 0.0f, 0.0f, -1.0f);
        payload.mask_param_back = float4(0.0f, 0.0f, 0.0f, -1.0f);
        payload.panel_optics_front = float4(uv, 0.0f, scene_edge);
        payload.panel_optics_back = float4(uv, 0.0f, scene_edge);
        payload.panel_profile_front = float4(0.0f, 0.0f, 0.0f, 0.0f);
        payload.panel_profile_back = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return payload;
    }

    const float center_depth = SampleDepthSafe(pixel);
    const float3 center_normal = SampleNormalSafe(pixel);
    float3 world_position = 0.0f;
    const bool valid_world_position = TryGetWorldPosition(pixel, center_depth, world_position);
    float3 view_dir = float3(0.0f, 0.0f, 1.0f);
    if (valid_world_position)
    {
        const float3 to_view = view_position.xyz - world_position;
        const float to_view_len_sq = dot(to_view, to_view);
        if (to_view_len_sq > 1e-6f)
        {
            view_dir = to_view * rsqrt(to_view_len_sq);
        }
    }
    const float n_dot_v = saturate(abs(dot(center_normal, view_dir)));

    float front_mask = 0.0f;
    float front_rim = 0.0f;
    float front_mixed_fresnel = 0.0f;
    float2 front_refraction_uv = uv;
    float front_effective_blur_strength = 0.0f;
    float2 front_profile_normal_uv = float2(0.0f, 0.0f);
    float front_optical_thickness = 0.0f;
    float front_layer = -1e20f;
    int front_panel_index = -1;

    float back_mask = 0.0f;
    float back_rim = 0.0f;
    float back_mixed_fresnel = 0.0f;
    float2 back_refraction_uv = uv;
    float back_effective_blur_strength = 0.0f;
    float2 back_profile_normal_uv = float2(0.0f, 0.0f);
    float back_optical_thickness = 0.0f;
    float back_layer = -1e20f;
    int back_panel_index = -1;

    [loop]
    for (uint panel_index = 0; panel_index < panel_count; ++panel_index)
    {
        const FrostedGlassPanelData panel_data = g_frosted_panels[panel_index];
        float panel_mask = 0.0f;
        float panel_rim = 0.0f;
        float panel_mixed_fresnel = 0.0f;
        float2 panel_refraction_uv = uv;
        float panel_effective_blur_strength = 0.0f;
        float2 panel_profile_normal_uv = float2(0.0f, 0.0f);
        float panel_optical_thickness = 0.0f;
        EvaluatePanelCandidatePayload(
            uv,
            center_depth,
            center_normal,
            n_dot_v,
            panel_data,
            panel_mask,
            panel_rim,
            panel_mixed_fresnel,
            panel_refraction_uv,
            panel_effective_blur_strength,
            panel_profile_normal_uv,
            panel_optical_thickness);
        if (panel_mask <= 1e-4f)
        {
            continue;
        }

        const float panel_layer = panel_data.layering_info.x;
        const int panel_index_signed = (int)panel_index;
        const bool better_than_front = IsBetterPanelCandidate(
            panel_layer,
            panel_mask,
            panel_index_signed,
            front_layer,
            front_mask,
            front_panel_index);
        if (better_than_front)
        {
            back_mask = front_mask;
            back_rim = front_rim;
            back_mixed_fresnel = front_mixed_fresnel;
            back_refraction_uv = front_refraction_uv;
            back_effective_blur_strength = front_effective_blur_strength;
            back_profile_normal_uv = front_profile_normal_uv;
            back_optical_thickness = front_optical_thickness;
            back_layer = front_layer;
            back_panel_index = front_panel_index;

            front_mask = panel_mask;
            front_rim = panel_rim;
            front_mixed_fresnel = panel_mixed_fresnel;
            front_refraction_uv = panel_refraction_uv;
            front_effective_blur_strength = panel_effective_blur_strength;
            front_profile_normal_uv = panel_profile_normal_uv;
            front_optical_thickness = panel_optical_thickness;
            front_layer = panel_layer;
            front_panel_index = panel_index_signed;
            continue;
        }

        const bool better_than_back = IsBetterPanelCandidate(
            panel_layer,
            panel_mask,
            panel_index_signed,
            back_layer,
            back_mask,
            back_panel_index);
        if (!better_than_back)
        {
            continue;
        }

        back_mask = panel_mask;
        back_rim = panel_rim;
        back_mixed_fresnel = panel_mixed_fresnel;
        back_refraction_uv = panel_refraction_uv;
        back_effective_blur_strength = panel_effective_blur_strength;
        back_profile_normal_uv = panel_profile_normal_uv;
        back_optical_thickness = panel_optical_thickness;
        back_layer = panel_layer;
        back_panel_index = panel_index_signed;
    }

    payload.mask_param_front = float4(front_mask, front_rim, front_mixed_fresnel, (float)front_panel_index);
    payload.mask_param_back = float4(back_mask, back_rim, back_mixed_fresnel, (float)back_panel_index);
    payload.panel_optics_front = float4(front_refraction_uv, front_effective_blur_strength, scene_edge);
    payload.panel_optics_back = float4(back_refraction_uv, back_effective_blur_strength, scene_edge);
    payload.panel_profile_front = float4(front_profile_normal_uv, front_rim, front_optical_thickness);
    payload.panel_profile_back = float4(back_profile_normal_uv, back_rim, back_optical_thickness);
    return payload;
}

[numthreads(8, 8, 1)]
void MaskParameterMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= viewport_width || dispatch_thread_id.y >= viewport_height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float2 uv = (float2(pixel) + 0.5f) / float2((float)viewport_width, (float)viewport_height);
    const PanelPayloadResult payload = EvaluatePanelPayloadAtPixel(pixel, uv);
    MaskParamOutput[pixel] = payload.mask_param_front;
    MaskParamSecondaryOutput[pixel] = payload.mask_param_back;
    PanelOpticsOutput[pixel] = payload.panel_optics_front;
    PanelOpticsSecondaryOutput[pixel] = payload.panel_optics_back;
    PanelProfileOutput[pixel] = payload.panel_profile_front;
    PanelProfileSecondaryOutput[pixel] = payload.panel_profile_back;
}

struct PanelPayloadVSOutput
{
    float4 position : SV_POSITION;
    float2 uv : UV;
    nointerpolation uint panel_index : PANEL_INDEX;
};

float EncodePanelLayerDepth(float layer_order)
{
    const float layer_min = -8.0f;
    const float layer_max = 8.0f;
    const float layer_t = saturate((layer_order - layer_min) / max(layer_max - layer_min, 1e-4f));
    return 1.0f - layer_t;
}

PanelPayloadVSOutput PanelPayloadVS(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    PanelPayloadVSOutput output;
    output.panel_index = instance_id;
    output.uv = float2(0.0f, 0.0f);
    output.position = float4(-2.0f, -2.0f, 1.0f, 1.0f);

    if (instance_id >= panel_count)
    {
        return output;
    }

    static const float2 k_quad_corners[6] = {
        float2(-1.0f, -1.0f),
        float2(1.0f, -1.0f),
        float2(-1.0f, 1.0f),
        float2(-1.0f, 1.0f),
        float2(1.0f, -1.0f),
        float2(1.0f, 1.0f)
    };

    const FrostedGlassPanelData panel_data = g_frosted_panels[instance_id];
    const float2 panel_center_uv = panel_data.center_half_size.xy;
    const float2 panel_half_size_uv = panel_data.center_half_size.zw;
    const float2 corner = k_quad_corners[vertex_id % 6];
    const float2 panel_uv = panel_center_uv + corner * panel_half_size_uv;
    const float2 clip_position = float2(panel_uv.x * 2.0f - 1.0f, 1.0f - panel_uv.y * 2.0f);
    const float panel_depth = EncodePanelLayerDepth(panel_data.layering_info.x);
    output.uv = panel_uv;
    output.position = float4(clip_position, panel_depth, 1.0f);
    return output;
}

bool EvaluateSinglePanelLayerPayload(int2 pixel,
                                     float2 uv,
                                     uint panel_index,
                                     FrostedGlassPanelData panel_data,
                                     out float4 mask_param,
                                     out float4 panel_optics,
                                     out float4 panel_profile)
{
    mask_param = float4(0.0f, 0.0f, 0.0f, -1.0f);
    panel_optics = float4(uv, 0.0f, 0.0f);
    panel_profile = float4(0.0f, 0.0f, 0.0f, 0.0f);

    const float depth_dx = abs(SampleDepthSafe(pixel + int2(1, 0)) - SampleDepthSafe(pixel - int2(1, 0)));
    const float depth_dy = abs(SampleDepthSafe(pixel + int2(0, 1)) - SampleDepthSafe(pixel - int2(0, 1)));
    const float scene_edge = saturate((depth_dx + depth_dy) * scene_edge_scale);

    const float center_depth = SampleDepthSafe(pixel);
    const float3 center_normal = SampleNormalSafe(pixel);
    float3 world_position = 0.0f;
    const bool valid_world_position = TryGetWorldPosition(pixel, center_depth, world_position);
    float3 view_dir = float3(0.0f, 0.0f, 1.0f);
    if (valid_world_position)
    {
        const float3 to_view = view_position.xyz - world_position;
        const float to_view_len_sq = dot(to_view, to_view);
        if (to_view_len_sq > 1e-6f)
        {
            view_dir = to_view * rsqrt(to_view_len_sq);
        }
    }
    const float n_dot_v = saturate(abs(dot(center_normal, view_dir)));

    float panel_mask = 0.0f;
    float panel_rim = 0.0f;
    float panel_mixed_fresnel = 0.0f;
    float2 panel_refraction_uv = uv;
    float panel_effective_blur_strength = 0.0f;
    float2 panel_profile_normal_uv = float2(0.0f, 0.0f);
    float panel_optical_thickness = 0.0f;
    EvaluatePanelCandidatePayload(
        uv,
        center_depth,
        center_normal,
        n_dot_v,
        panel_data,
        panel_mask,
        panel_rim,
        panel_mixed_fresnel,
        panel_refraction_uv,
        panel_effective_blur_strength,
        panel_profile_normal_uv,
        panel_optical_thickness);
    if (panel_mask <= 1e-4f)
    {
        return false;
    }

    mask_param = float4(panel_mask, panel_rim, panel_mixed_fresnel, (float)panel_index);
    panel_optics = float4(panel_refraction_uv, panel_effective_blur_strength, scene_edge);
    panel_profile = float4(panel_profile_normal_uv, panel_rim, panel_optical_thickness);
    return true;
}

struct PanelPayloadLayerPSOutput
{
    float4 mask_param : SV_TARGET0;
    float4 panel_optics : SV_TARGET1;
    float4 panel_profile : SV_TARGET2;
};

PanelPayloadLayerPSOutput PanelPayloadFrontPS(PanelPayloadVSOutput input)
{
    const int2 pixel = ClampToViewport(int2(input.position.xy));
    const uint panel_index = input.panel_index;
    if (panel_index >= panel_count)
    {
        discard;
    }
    const FrostedGlassPanelData panel_data = g_frosted_panels[panel_index];

    float4 mask_param = float4(0.0f, 0.0f, 0.0f, -1.0f);
    float4 panel_optics = float4(input.uv, 0.0f, 0.0f);
    float4 panel_profile = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (!EvaluateSinglePanelLayerPayload(pixel, input.uv, panel_index, panel_data, mask_param, panel_optics, panel_profile))
    {
        discard;
    }

    PanelPayloadLayerPSOutput output;
    output.mask_param = mask_param;
    output.panel_optics = panel_optics;
    output.panel_profile = panel_profile;
    return output;
}

PanelPayloadLayerPSOutput PanelPayloadBackPS(PanelPayloadVSOutput input)
{
    const int2 pixel = ClampToViewport(int2(input.position.xy));
    const uint panel_index = input.panel_index;
    if (panel_index >= panel_count)
    {
        discard;
    }
    const float4 front_mask_param = FrontMaskParamTex.Load(int3(pixel, 0));
    const float front_mask = saturate(front_mask_param.x);
    const int front_panel_index = (int)round(front_mask_param.w);
    if (front_mask > 1e-4f && front_panel_index == (int)panel_index)
    {
        discard;
    }

    const FrostedGlassPanelData panel_data = g_frosted_panels[panel_index];
    float4 mask_param = float4(0.0f, 0.0f, 0.0f, -1.0f);
    float4 panel_optics = float4(input.uv, 0.0f, 0.0f);
    float4 panel_profile = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (!EvaluateSinglePanelLayerPayload(pixel, input.uv, panel_index, panel_data, mask_param, panel_optics, panel_profile))
    {
        discard;
    }

    PanelPayloadLayerPSOutput output;
    output.mask_param = mask_param;
    output.panel_optics = panel_optics;
    output.panel_profile = panel_profile;
    return output;
}

[numthreads(8, 8, 1)]
void CompositeBackMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= viewport_width || dispatch_thread_id.y >= viewport_height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float3 scene_color = InputColorTex.Load(int3(pixel, 0)).rgb;
    const float4 front_mask_param = MaskParamTex.Load(int3(pixel, 0));
    const float4 back_mask_param = MaskParamSecondaryTex.Load(int3(pixel, 0));
    const float4 back_panel_optics = PanelOpticsSecondaryTex.Load(int3(pixel, 0));
    const float4 front_panel_profile = PanelProfileTex.Load(int3(pixel, 0));
    const float4 back_panel_profile = PanelProfileSecondaryTex.Load(int3(pixel, 0));

    float front_mask = 0.0f;
    const bool has_front_layer = TryResolvePanelPayload(front_mask_param, front_mask);

    float back_mask = 0.0f;
    float3 back_frosted_color = scene_color;
    float back_blur_metric = 0.0f;
    const bool has_back_layer = EvaluatePanelFrostedColor(
        scene_color,
        back_mask_param,
        back_panel_optics,
        back_mask,
        back_frosted_color,
        back_blur_metric);

    float3 back_composited_color = scene_color;
    if (ShouldEnableMultilayer(has_front_layer, has_back_layer, back_mask))
    {
        const float effective_back_mask = ComputeEffectiveBackMask(back_mask, front_mask);
        const float profile_layer_hint = saturate(max(front_panel_profile.z, back_panel_profile.z));
        const float layered_mix = saturate(effective_back_mask + profile_layer_hint * 1e-4f);
        back_composited_color = lerp(scene_color, back_frosted_color, layered_mix);
    }

    BackCompositeOutput[pixel] = float4(back_composited_color, 1.0f);
}

[numthreads(8, 8, 1)]
void CompositeFrontMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= viewport_width || dispatch_thread_id.y >= viewport_height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float2 viewport_size = float2((float)viewport_width, (float)viewport_height);
    const float2 current_uv = (float2(pixel) + 0.5f) / viewport_size;
    const float3 scene_color = BackCompositeTex.Load(int3(pixel, 0)).rgb;
    const float4 front_mask_param = MaskParamTex.Load(int3(pixel, 0));
    const float4 front_panel_optics = PanelOpticsTex.Load(int3(pixel, 0));
    const float4 back_mask_param = MaskParamSecondaryTex.Load(int3(pixel, 0));
    const float4 back_panel_optics = PanelOpticsSecondaryTex.Load(int3(pixel, 0));
    const float4 front_panel_profile = PanelProfileTex.Load(int3(pixel, 0));
    const float4 back_panel_profile = PanelProfileSecondaryTex.Load(int3(pixel, 0));
    const float profile_edge = saturate(max(front_panel_profile.z, back_panel_profile.z));
    const float scene_edge = saturate(max(max(front_panel_optics.w, back_panel_optics.w), profile_edge));

    float front_mask = 0.0f;
    float3 front_frosted_color = scene_color;
    float front_blur_metric = 0.0f;
    const bool has_front_layer = EvaluatePanelFrostedColor(
        scene_color,
        front_mask_param,
        front_panel_optics,
        front_mask,
        front_frosted_color,
        front_blur_metric);

    float back_mask = 0.0f;
    const bool has_back_layer = TryResolvePanelPayload(back_mask_param, back_mask);

    float panel_mask = front_mask;
    float3 final_color = has_front_layer ? lerp(scene_color, front_frosted_color, front_mask) : scene_color;

    if (ShouldEnableMultilayer(has_front_layer, has_back_layer, back_mask))
    {
        const float effective_back_mask = ComputeEffectiveBackMask(back_mask, front_mask);
        panel_mask = saturate(front_mask + effective_back_mask);
    }

    float3 history_color = final_color;
    float history_weight = 0.0f;
    if (temporal_history_valid != 0 && panel_mask > 1e-4f)
    {
        const float2 velocity_uv = VelocityTex.Load(int3(pixel, 0)).xy;
        const float2 prev_uv = current_uv - velocity_uv;
        const bool history_in_bounds = all(prev_uv >= float2(0.0f, 0.0f)) && all(prev_uv <= float2(1.0f, 1.0f));
        if (history_in_bounds)
        {
            const int2 prev_pixel = ClampToViewport(int2(prev_uv * viewport_size));
            history_color = HistoryInputTex.Load(int3(prev_pixel, 0)).rgb;

            const float safe_velocity_threshold = max(temporal_reject_velocity, 1e-4f);
            const float velocity_reject = saturate(length(velocity_uv) / safe_velocity_threshold);
            const float edge_reject = saturate(scene_edge * temporal_edge_reject);
            history_weight = saturate(temporal_history_blend * (1.0f - velocity_reject) * (1.0f - edge_reject) * panel_mask);
        }
    }

    const float3 stabilized_color = lerp(final_color, history_color, history_weight);
    Output[pixel] = float4(stabilized_color, 1.0f);
    HistoryOutputTex[pixel] = float4(stabilized_color, 1.0f);
}

[numthreads(8, 8, 1)]
void CompositeMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= viewport_width || dispatch_thread_id.y >= viewport_height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float2 viewport_size = float2((float)viewport_width, (float)viewport_height);
    const float2 current_uv = (float2(pixel) + 0.5f) / viewport_size;
    const float3 scene_color = InputColorTex.Load(int3(pixel, 0)).rgb;
    const float4 front_mask_param = MaskParamTex.Load(int3(pixel, 0));
    const float4 front_panel_optics = PanelOpticsTex.Load(int3(pixel, 0));
    const float4 back_mask_param = MaskParamSecondaryTex.Load(int3(pixel, 0));
    const float4 back_panel_optics = PanelOpticsSecondaryTex.Load(int3(pixel, 0));
    const float4 front_panel_profile = PanelProfileTex.Load(int3(pixel, 0));
    const float4 back_panel_profile = PanelProfileSecondaryTex.Load(int3(pixel, 0));
    const float profile_edge = saturate(max(front_panel_profile.z, back_panel_profile.z));
    const float scene_edge = saturate(max(max(front_panel_optics.w, back_panel_optics.w), profile_edge));

    float front_mask = 0.0f;
    float3 front_frosted_color = scene_color;
    float front_blur_metric = 0.0f;
    const bool has_front_layer = EvaluatePanelFrostedColor(
        scene_color,
        front_mask_param,
        front_panel_optics,
        front_mask,
        front_frosted_color,
        front_blur_metric);

    float back_mask = 0.0f;
    float3 back_frosted_color = scene_color;
    float back_blur_metric = 0.0f;
    const bool has_back_layer = EvaluatePanelFrostedColor(
        scene_color,
        back_mask_param,
        back_panel_optics,
        back_mask,
        back_frosted_color,
        back_blur_metric);

    float panel_mask = front_mask;
    float3 final_color = lerp(scene_color, front_frosted_color, front_mask);
    const bool enable_multilayer = ShouldEnableMultilayer(has_front_layer, has_back_layer, back_mask);

    if (enable_multilayer)
    {
        const float effective_back_mask = ComputeEffectiveBackMask(back_mask, front_mask);
        const float3 back_composited_color = lerp(scene_color, back_frosted_color, effective_back_mask);

        float front_mask_over_back = 0.0f;
        float3 front_over_back_frosted_color = back_composited_color;
        float front_over_back_blur_metric = 0.0f;
        const bool has_front_over_back = EvaluatePanelFrostedColor(
            back_composited_color,
            front_mask_param,
            front_panel_optics,
            front_mask_over_back,
            front_over_back_frosted_color,
            front_over_back_blur_metric);

        if (has_front_over_back && front_mask_over_back > 1e-4f)
        {
            float front_effective_mix = front_mask_over_back;
            if (back_blur_metric > front_over_back_blur_metric + 1e-4f)
            {
                const float blur_ratio = saturate(front_over_back_blur_metric / max(back_blur_metric, 1e-4f));
                const float min_front_mix_scale = saturate(multilayer_front_transmittance);
                front_effective_mix *= lerp(min_front_mix_scale, 1.0f, blur_ratio);
            }

            final_color = lerp(back_composited_color, front_over_back_frosted_color, front_effective_mix);
            panel_mask = saturate(front_effective_mix + effective_back_mask);
        }
        else
        {
            final_color = back_composited_color;
            panel_mask = effective_back_mask;
        }
    }

    float3 history_color = final_color;
    float history_weight = 0.0f;
    if (temporal_history_valid != 0 && panel_mask > 1e-4f)
    {
        const float2 velocity_uv = VelocityTex.Load(int3(pixel, 0)).xy;
        const float2 prev_uv = current_uv - velocity_uv;
        const bool history_in_bounds = all(prev_uv >= float2(0.0f, 0.0f)) && all(prev_uv <= float2(1.0f, 1.0f));
        if (history_in_bounds)
        {
            const int2 prev_pixel = ClampToViewport(int2(prev_uv * viewport_size));
            history_color = HistoryInputTex.Load(int3(prev_pixel, 0)).rgb;

            const float safe_velocity_threshold = max(temporal_reject_velocity, 1e-4f);
            const float velocity_reject = saturate(length(velocity_uv) / safe_velocity_threshold);
            const float edge_reject = saturate(scene_edge * temporal_edge_reject);
            history_weight = saturate(temporal_history_blend * (1.0f - velocity_reject) * (1.0f - edge_reject) * panel_mask);
        }
    }

    const float3 stabilized_color = lerp(final_color, history_color, history_weight);
    Output[pixel] = float4(stabilized_color, 1.0f);
    HistoryOutputTex[pixel] = float4(stabilized_color, 1.0f);
}

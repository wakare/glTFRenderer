#define BINDLESS_TEXTURE_SPACE_BAKER_MATERIAL space13

RaytracingAccelerationStructure g_scene_as : register(t0);

struct BakeTexelRecord
{
    float4 world_position_and_valid;
    float4 geometric_normal_and_padding;
};

struct BakeSceneVertex
{
    float4 world_position;
    float4 world_normal;
    float4 world_tangent;
    float4 texcoord0_texcoord1;
};

struct BakeSceneInstance
{
    uint4 offsets_and_flags;
    uint4 texture_indices_and_texcoords;
    uint4 texture_indices_and_texcoords_extra;
    float4 base_color;
    float4 emissive_and_roughness;
    float4 metallic_alpha_normal_and_padding;
};

struct BakeSceneLight
{
    float4 position_and_type;
    float4 direction_and_range;
    float4 color_and_intensity;
    float4 spot_angles;
};

struct BakeSceneEmissiveTriangle
{
    uint4 instance_and_primitive;
    float4 area_pdf_cdf_luminance;
};

StructuredBuffer<BakeTexelRecord> g_bake_texel_records : register(t1);
StructuredBuffer<BakeSceneVertex> g_scene_vertices : register(t2);
StructuredBuffer<uint> g_scene_indices : register(t3);
StructuredBuffer<BakeSceneInstance> g_scene_instances : register(t4);
StructuredBuffer<BakeSceneLight> g_scene_lights : register(t5);
StructuredBuffer<BakeSceneEmissiveTriangle> g_scene_emissive_triangles : register(t6);
Texture2D<float4> bindless_bake_material_textures[] : register(t0, BINDLESS_TEXTURE_SPACE_BAKER_MATERIAL);
RWTexture2D<float4> g_bake_output : register(u0);

cbuffer g_bake_dispatch_constants : register(b0)
{
    uint atlas_width;
    uint atlas_height;
    uint sample_index;
    uint sample_count;
    uint max_bounces;
    uint direct_light_sample_count;
    uint environment_light_sample_count;
    uint scene_light_count;
    uint emissive_triangle_count;
    uint padding_uint0;
    uint padding_uint1;
    uint padding_uint2;
    float ray_epsilon;
    float sky_intensity;
    float sky_ground_mix;
    float padding0;
    float4 environment_importance_cdf[8];
};

struct [raypayload] BakePayload
{
    float4 radiance_and_hit : read(caller) : write(caller, miss, closesthit);
    float4 normal_and_distance : read(caller) : write(caller, closesthit, miss);
    float4 albedo_and_padding : read(caller) : write(caller, closesthit, miss);
    float4 sampling_pdf_and_flags : read(caller) : write(caller, closesthit, miss);
};

static const float kPi = 3.14159265359f;
static const float kInvPi = 0.31830988618f;
static const uint kInstanceFlagDoubleSided = 1u;
static const uint kInstanceFlagAlphaMasked = 1u << 1u;
static const uint kMaterialTextureInvalidIndex = 0xffffffffu;
static const uint kSceneLightTypeDirectional = 0u;
static const uint kSceneLightTypePoint = 1u;
static const uint kSceneLightTypeSpot = 2u;
static const uint kEnvironmentImportanceBinCount = 32u;
static const float4 kTracePayloadSentinel = float4(-1.0f, -2.0f, -3.0f, -4.0f);

uint3 LoadTriangleVertexIndices(uint instance_id, uint primitive_index);
float3 InterpolateFloat3(float3 value0, float3 value1, float3 value2, float3 barycentrics);
float2 SelectMaterialTexcoord(BakeSceneVertex vertex0,
                              BakeSceneVertex vertex1,
                              BakeSceneVertex vertex2,
                              float3 barycentrics,
                              uint texcoord_index);
float SampleBaseAlpha(BakeSceneInstance scene_instance, float2 material_uv);
float3 SampleEmissive(BakeSceneInstance scene_instance, float2 material_uv);

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
    return (state & 0x00ffffffu) / 16777216.0f;
}

float ComputeLuminance(float3 color)
{
    return max(color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f, 0.0f);
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    const float length_sq = dot(value, value);
    if (length_sq <= 1e-12f)
    {
        return normalize(fallback);
    }

    return value * rsqrt(length_sq);
}

void BuildOrthonormalBasis(float3 n, out float3 tangent, out float3 bitangent)
{
    const float sign_value = n.z >= 0.0f ? 1.0f : -1.0f;
    const float a = -1.0f / (sign_value + n.z);
    const float b = n.x * n.y * a;
    tangent = float3(1.0f + sign_value * n.x * n.x * a, sign_value * b, -sign_value * n.x);
    bitangent = float3(b, sign_value + n.y * n.y * a, -n.y);
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

float PowerHeuristic(float a, float b)
{
    const float a2 = a * a;
    const float b2 = b * b;
    return a2 / max(a2 + b2, 1e-6f);
}

float GetEnvironmentImportanceCdf(uint bin_index)
{
    const uint packed_index = bin_index >> 2u;
    const uint component_index = bin_index & 3u;
    return environment_importance_cdf[packed_index][component_index];
}

float GetEnvironmentImportanceBinMass(uint bin_index)
{
    const float cumulative = GetEnvironmentImportanceCdf(bin_index);
    const float previous = bin_index > 0u ? GetEnvironmentImportanceCdf(bin_index - 1u) : 0.0f;
    return max(cumulative - previous, 0.0f);
}

float EvaluateEnvironmentImportancePdf(float3 world_direction)
{
    if (sky_intensity <= 0.0f)
    {
        return 0.0f;
    }

    const float normalized_y = saturate(world_direction.y * 0.5f + 0.5f);
    const uint bin_index = min((uint)(normalized_y * (float)kEnvironmentImportanceBinCount), kEnvironmentImportanceBinCount - 1u);
    const float bin_mass = GetEnvironmentImportanceBinMass(bin_index);
    if (bin_mass <= 0.0f)
    {
        return 0.0f;
    }

    const float bin_width = 2.0f / (float)kEnvironmentImportanceBinCount;
    const float pdf_y = bin_mass / bin_width;
    return pdf_y / (2.0f * kPi);
}

float3 SampleEnvironmentDirection(float2 sample_uv, out float out_pdf)
{
    uint bin_index = 0u;
    [loop]
    for (uint current_bin_index = 0u; current_bin_index < kEnvironmentImportanceBinCount; ++current_bin_index)
    {
        bin_index = current_bin_index;
        if (sample_uv.x <= GetEnvironmentImportanceCdf(current_bin_index) || current_bin_index + 1u >= kEnvironmentImportanceBinCount)
        {
            break;
        }
    }

    const float previous_cdf = bin_index > 0u ? GetEnvironmentImportanceCdf(bin_index - 1u) : 0.0f;
    const float current_cdf = GetEnvironmentImportanceCdf(bin_index);
    const float bin_mass = max(current_cdf - previous_cdf, 1e-6f);
    const float local_u = saturate((sample_uv.x - previous_cdf) / bin_mass);
    const float bin_width = 2.0f / (float)kEnvironmentImportanceBinCount;
    const float direction_y = -1.0f + ((float)bin_index + local_u) * bin_width;
    const float phi = 2.0f * kPi * sample_uv.y;
    const float radius = sqrt(max(0.0f, 1.0f - direction_y * direction_y));
    out_pdf = (bin_mass / bin_width) / (2.0f * kPi);
    return float3(radius * cos(phi), direction_y, radius * sin(phi));
}

float EvaluateRangeAttenuation(float distance_sq, float light_range)
{
    if (distance_sq <= 1e-8f)
    {
        return 0.0f;
    }

    if (light_range > 0.0f && distance_sq > light_range * light_range)
    {
        return 0.0f;
    }

    return rcp(distance_sq);
}

float EvaluateSpotAttenuation(BakeSceneLight light, float3 light_direction)
{
    const float outer_cos = saturate(light.spot_angles.y);
    const float inner_cos = max(light.spot_angles.x, outer_cos);
    const float3 spot_direction = SafeNormalize(light.direction_and_range.xyz, float3(0.0f, 0.0f, -1.0f));
    const float cos_theta = dot(spot_direction, light_direction);
    if (cos_theta <= outer_cos)
    {
        return 0.0f;
    }

    const float angle_falloff = saturate((cos_theta - outer_cos) / max(inner_cos - outer_cos, 1e-4f));
    return angle_falloff * angle_falloff;
}

bool TraceShadowVisibility(float3 shadow_origin, float3 shadow_direction, float ray_max_distance)
{
    if (ray_max_distance <= ray_epsilon)
    {
        return false;
    }

    RayDesc shadow_ray;
    shadow_ray.Origin = shadow_origin;
    shadow_ray.Direction = shadow_direction;
    shadow_ray.TMin = ray_epsilon;
    shadow_ray.TMax = ray_max_distance;

    BakePayload shadow_payload;
    shadow_payload.radiance_and_hit = kTracePayloadSentinel;
    shadow_payload.normal_and_distance = float4(0.0f, 1.0f, 0.0f, 0.0f);
    shadow_payload.albedo_and_padding = float4(0.0f, 0.0f, 0.0f, 0.0f);
    shadow_payload.sampling_pdf_and_flags = float4(0.0f, 0.0f, 0.0f, 0.0f);

    TraceRay(
        g_scene_as,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xff,
        0,
        1,
        0,
        shadow_ray,
        shadow_payload);

    return shadow_payload.radiance_and_hit.a < 0.5f;
}

float3 EvaluateLightContribution(uint light_index, float3 hit_position, float3 hit_normal, float3 lambert_brdf)
{
    const BakeSceneLight light = g_scene_lights[light_index];
    const uint light_type = (uint)(light.position_and_type.w + 0.5f);
    const float3 light_color = max(light.color_and_intensity.rgb, 0.0f);
    const float light_intensity = max(light.color_and_intensity.w, 0.0f);
    if (light_intensity <= 0.0f || all(light_color <= 0.0f))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 light_direction = float3(0.0f, 1.0f, 0.0f);
    float light_attenuation = 1.0f;
    float shadow_distance = 10000.0f;

    if (light_type == kSceneLightTypeDirectional)
    {
        light_direction = SafeNormalize(-light.direction_and_range.xyz, float3(0.0f, 1.0f, 0.0f));
    }
    else
    {
        const float3 to_light = light.position_and_type.xyz - hit_position;
        const float distance_sq = dot(to_light, to_light);
        if (distance_sq <= 1e-8f)
        {
            return float3(0.0f, 0.0f, 0.0f);
        }

        const float distance_to_light = sqrt(distance_sq);
        light_direction = to_light / distance_to_light;
        shadow_distance = max(distance_to_light - ray_epsilon, ray_epsilon);
        light_attenuation = EvaluateRangeAttenuation(distance_sq, light.direction_and_range.w);
        if (light_attenuation <= 0.0f)
        {
            return float3(0.0f, 0.0f, 0.0f);
        }

        if (light_type == kSceneLightTypeSpot)
        {
            const float spot_attenuation = EvaluateSpotAttenuation(light, -light_direction);
            if (spot_attenuation <= 0.0f)
            {
                return float3(0.0f, 0.0f, 0.0f);
            }

            light_attenuation *= spot_attenuation;
        }
    }

    const float n_dot_l = saturate(dot(hit_normal, light_direction));
    if (n_dot_l <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    if (!TraceShadowVisibility(hit_position + hit_normal * ray_epsilon, light_direction, shadow_distance))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 incident_radiance = light_color * (light_intensity * light_attenuation);
    return lambert_brdf * incident_radiance * n_dot_l;
}

float GetLightSelectionPdf(uint light_index)
{
    return max(g_scene_lights[light_index].spot_angles.z, 0.0f);
}

uint SampleLightIndex(float sample_value)
{
    [loop]
    for (uint light_index = 0u; light_index < scene_light_count; ++light_index)
    {
        if (sample_value <= g_scene_lights[light_index].spot_angles.w || light_index + 1u >= scene_light_count)
        {
            return light_index;
        }
    }

    return 0u;
}

float3 EvaluateDirectLighting(float3 hit_position, float3 hit_normal, float3 albedo, inout uint rng_state)
{
    if (direct_light_sample_count == 0u || scene_light_count == 0u)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 lambert_brdf = albedo * rcp(kPi);
    const uint sampled_light_count = min(direct_light_sample_count, scene_light_count);
    float3 direct_radiance = float3(0.0f, 0.0f, 0.0f);

    if (sampled_light_count >= scene_light_count)
    {
        [loop]
        for (uint light_index = 0u; light_index < scene_light_count; ++light_index)
        {
            direct_radiance += EvaluateLightContribution(light_index, hit_position, hit_normal, lambert_brdf);
        }

        return direct_radiance;
    }

    [loop]
    for (uint sample_light_index = 0u; sample_light_index < sampled_light_count; ++sample_light_index)
    {
        const uint light_index = SampleLightIndex(Random01(rng_state));
        const float light_pdf = max(GetLightSelectionPdf(light_index), 1e-6f);
        direct_radiance +=
            EvaluateLightContribution(light_index, hit_position, hit_normal, lambert_brdf) /
            (light_pdf * sampled_light_count);
    }

    return direct_radiance;
}

float GetEmissiveTriangleSelectionPdf(uint triangle_index)
{
    return max(g_scene_emissive_triangles[triangle_index].area_pdf_cdf_luminance.y, 0.0f);
}

bool TryGetEmissiveTriangleSamplingData(uint instance_id,
                                        uint primitive_index,
                                        out float out_triangle_area,
                                        out float out_selection_pdf)
{
    out_triangle_area = 0.0f;
    out_selection_pdf = 0.0f;

    [loop]
    for (uint triangle_index = 0u; triangle_index < emissive_triangle_count; ++triangle_index)
    {
        const BakeSceneEmissiveTriangle emissive_triangle = g_scene_emissive_triangles[triangle_index];
        if (emissive_triangle.instance_and_primitive.x != instance_id ||
            emissive_triangle.instance_and_primitive.y != primitive_index)
        {
            continue;
        }

        out_triangle_area = max(emissive_triangle.area_pdf_cdf_luminance.x, 0.0f);
        out_selection_pdf = GetEmissiveTriangleSelectionPdf(triangle_index);
        return out_triangle_area > 1e-8f && out_selection_pdf > 1e-8f;
    }

    return false;
}

float EvaluateEmissiveTriangleDirectPdf(float triangle_area,
                                        float selection_pdf,
                                        float ray_distance,
                                        float light_cosine)
{
    if (triangle_area <= 1e-8f || selection_pdf <= 1e-8f || ray_distance <= ray_epsilon || light_cosine <= 1e-6f)
    {
        return 0.0f;
    }

    const float area_pdf = selection_pdf / triangle_area;
    return area_pdf * ((ray_distance * ray_distance) / light_cosine);
}

uint SampleEmissiveTriangleIndex(float sample_value)
{
    [loop]
    for (uint triangle_index = 0u; triangle_index < emissive_triangle_count; ++triangle_index)
    {
        if (sample_value <= g_scene_emissive_triangles[triangle_index].area_pdf_cdf_luminance.z ||
            triangle_index + 1u >= emissive_triangle_count)
        {
            return triangle_index;
        }
    }

    return 0u;
}

float3 SampleTriangleBarycentrics(float2 sample_uv)
{
    const float sqrt_u = sqrt(saturate(sample_uv.x));
    const float barycentric0 = 1.0f - sqrt_u;
    const float barycentric1 = sample_uv.y * sqrt_u;
    return float3(barycentric0, barycentric1, 1.0f - barycentric0 - barycentric1);
}

float3 EvaluateEmissiveTriangleContribution(uint triangle_index,
                                            float3 hit_position,
                                            float3 hit_normal,
                                            float3 lambert_brdf,
                                            bool has_bsdf_competition,
                                            inout uint rng_state)
{
    const BakeSceneEmissiveTriangle emissive_triangle = g_scene_emissive_triangles[triangle_index];
    const float triangle_area = max(emissive_triangle.area_pdf_cdf_luminance.x, 0.0f);
    const float selection_pdf = GetEmissiveTriangleSelectionPdf(triangle_index);
    if (triangle_area <= 1e-8f || selection_pdf <= 1e-8f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const uint instance_id = emissive_triangle.instance_and_primitive.x;
    const uint primitive_index = emissive_triangle.instance_and_primitive.y;
    const BakeSceneInstance scene_instance = g_scene_instances[instance_id];
    if (ComputeLuminance(max(scene_instance.emissive_and_roughness.rgb, 0.0f)) <= 1e-8f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const uint3 triangle_indices = LoadTriangleVertexIndices(instance_id, primitive_index);
    const BakeSceneVertex vertex0 = g_scene_vertices[triangle_indices.x];
    const BakeSceneVertex vertex1 = g_scene_vertices[triangle_indices.y];
    const BakeSceneVertex vertex2 = g_scene_vertices[triangle_indices.z];
    const float3 barycentrics = SampleTriangleBarycentrics(float2(Random01(rng_state), Random01(rng_state)));
    const float3 light_position = InterpolateFloat3(
        vertex0.world_position.xyz,
        vertex1.world_position.xyz,
        vertex2.world_position.xyz,
        barycentrics);
    const float2 base_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords.y);
    const float2 emissive_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords.w);

    if ((scene_instance.offsets_and_flags.w & kInstanceFlagAlphaMasked) != 0u)
    {
        const float surface_alpha = SampleBaseAlpha(scene_instance, base_uv);
        const float alpha_cutoff = saturate(scene_instance.metallic_alpha_normal_and_padding.y);
        if (surface_alpha < alpha_cutoff)
        {
            return float3(0.0f, 0.0f, 0.0f);
        }
    }

    const float3 emissive = SampleEmissive(scene_instance, emissive_uv);
    if (all(emissive <= 0.0f))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 to_light = light_position - hit_position;
    const float distance_sq = dot(to_light, to_light);
    if (distance_sq <= 1e-8f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float distance_to_light = sqrt(distance_sq);
    const float3 light_direction = to_light / distance_to_light;
    const float n_dot_l = saturate(dot(hit_normal, light_direction));
    if (n_dot_l <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 light_edge01 = vertex1.world_position.xyz - vertex0.world_position.xyz;
    const float3 light_edge02 = vertex2.world_position.xyz - vertex0.world_position.xyz;
    const float3 light_geometric_normal = SafeNormalize(cross(light_edge01, light_edge02), float3(0.0f, 1.0f, 0.0f));
    const float light_cosine = abs(dot(light_geometric_normal, -light_direction));
    if (light_cosine <= 1e-6f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 shadow_origin = hit_position + hit_normal * ray_epsilon + light_direction * ray_epsilon;
    const float shadow_distance = max(length(light_position - shadow_origin) - ray_epsilon, ray_epsilon);
    if (!TraceShadowVisibility(shadow_origin, light_direction, shadow_distance))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float area_pdf = selection_pdf / triangle_area;
    const float direct_pdf = max((distance_sq * area_pdf) / light_cosine, 1e-6f);
    float mis_weight = 1.0f;
    if (has_bsdf_competition)
    {
        const float bsdf_pdf = n_dot_l * kInvPi;
        mis_weight = PowerHeuristic((float)direct_light_sample_count * direct_pdf, bsdf_pdf);
    }

    return lambert_brdf * emissive * (n_dot_l * mis_weight / direct_pdf);
}

float3 EvaluateEmissiveDirectLighting(float3 hit_position,
                                      float3 hit_normal,
                                      float3 albedo,
                                      bool skip_emissive_sampling,
                                      bool has_bsdf_competition,
                                      inout uint rng_state)
{
    if (skip_emissive_sampling || direct_light_sample_count == 0u || emissive_triangle_count == 0u)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    const float3 lambert_brdf = albedo * kInvPi;
    float3 direct_radiance = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint sample_triangle_index = 0u;
         sample_triangle_index < direct_light_sample_count;
         ++sample_triangle_index)
    {
        const uint triangle_index = SampleEmissiveTriangleIndex(Random01(rng_state));
        direct_radiance += EvaluateEmissiveTriangleContribution(
            triangle_index,
            hit_position,
            hit_normal,
            lambert_brdf,
            has_bsdf_competition,
            rng_state);
    }

    return direct_radiance / max((float)direct_light_sample_count, 1.0f);
}

float3 EvaluateEnvironmentLighting(float3 hit_position,
                                   float3 hit_normal,
                                   float3 albedo,
                                   uint sampled_environment_light_count,
                                   bool has_bsdf_competition,
                                   inout uint rng_state)
{
    if (sampled_environment_light_count == 0u || sky_intensity <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 lambert_brdf = albedo * kInvPi;
    float3 environment_radiance = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (uint environment_sample_index = 0u;
         environment_sample_index < sampled_environment_light_count;
         ++environment_sample_index)
    {
        float environment_pdf = 0.0f;
        const float3 environment_direction = SampleEnvironmentDirection(
            float2(Random01(rng_state), Random01(rng_state)),
            environment_pdf);
        if (environment_pdf <= 1e-6f)
        {
            continue;
        }

        const float n_dot_l = saturate(dot(hit_normal, environment_direction));
        if (n_dot_l <= 0.0f)
        {
            continue;
        }

        if (!TraceShadowVisibility(hit_position + hit_normal * ray_epsilon, environment_direction, 10000.0f))
        {
            continue;
        }

        float mis_weight = 1.0f;
        if (has_bsdf_competition)
        {
            const float bsdf_pdf = n_dot_l * kInvPi;
            mis_weight = PowerHeuristic((float)sampled_environment_light_count * environment_pdf, bsdf_pdf);
        }

        environment_radiance +=
            lambert_brdf * EvaluateSkyRadiance(environment_direction) * (n_dot_l * mis_weight / environment_pdf);
    }

    return environment_radiance / max((float)sampled_environment_light_count, 1.0f);
}

uint3 LoadTriangleVertexIndices(uint instance_id, uint primitive_index)
{
    const BakeSceneInstance scene_instance = g_scene_instances[instance_id];
    const uint vertex_offset = scene_instance.offsets_and_flags.x;
    const uint index_offset = scene_instance.offsets_and_flags.y + primitive_index * 3u;

    return uint3(
        vertex_offset + g_scene_indices[index_offset + 0u],
        vertex_offset + g_scene_indices[index_offset + 1u],
        vertex_offset + g_scene_indices[index_offset + 2u]);
}

float3 InterpolateFloat3(float3 value0, float3 value1, float3 value2, float3 barycentrics)
{
    return value0 * barycentrics.x + value1 * barycentrics.y + value2 * barycentrics.z;
}

float2 InterpolateFloat2(float2 value0, float2 value1, float2 value2, float3 barycentrics)
{
    return value0 * barycentrics.x + value1 * barycentrics.y + value2 * barycentrics.z;
}

float2 SelectMaterialTexcoord(BakeSceneVertex vertex0,
                              BakeSceneVertex vertex1,
                              BakeSceneVertex vertex2,
                              float3 barycentrics,
                              uint texcoord_index)
{
    if (texcoord_index == 1u)
    {
        return InterpolateFloat2(
            vertex0.texcoord0_texcoord1.zw,
            vertex1.texcoord0_texcoord1.zw,
            vertex2.texcoord0_texcoord1.zw,
            barycentrics);
    }

    return InterpolateFloat2(
        vertex0.texcoord0_texcoord1.xy,
        vertex1.texcoord0_texcoord1.xy,
        vertex2.texcoord0_texcoord1.xy,
        barycentrics);
}

float4 SampleMaterialTextureRGBA(uint texture_index, float2 material_uv, float4 fallback_value)
{
    if (texture_index == kMaterialTextureInvalidIndex)
    {
        return fallback_value;
    }

    uint texture_width = 1u;
    uint texture_height = 1u;
    bindless_bake_material_textures[texture_index].GetDimensions(texture_width, texture_height);
    const float2 clamped_uv = saturate(material_uv);
    const float2 pixel_coord = min(
        clamped_uv * float2(texture_width, texture_height),
        float2(texture_width - 1u, texture_height - 1u));
    return bindless_bake_material_textures[texture_index].Load(int3(uint2(pixel_coord), 0));
}

float4 SampleBaseColorRGBA(BakeSceneInstance scene_instance, float2 material_uv)
{
    const uint base_color_texture_index = scene_instance.texture_indices_and_texcoords.x;
    const float4 base_color_factor = max(scene_instance.base_color, 0.0f);
    const float4 sampled_base_color = SampleMaterialTextureRGBA(
        base_color_texture_index,
        material_uv,
        float4(1.0f, 1.0f, 1.0f, 1.0f));
    return max(sampled_base_color * base_color_factor, 0.0f);
}

float3 SampleBaseColor(BakeSceneInstance scene_instance, float2 material_uv)
{
    return SampleBaseColorRGBA(scene_instance, material_uv).rgb;
}

float SampleBaseAlpha(BakeSceneInstance scene_instance, float2 material_uv)
{
    return saturate(SampleBaseColorRGBA(scene_instance, material_uv).a);
}

float3 SampleEmissive(BakeSceneInstance scene_instance, float2 material_uv)
{
    const float3 emissive_factor = max(scene_instance.emissive_and_roughness.rgb, 0.0f);
    const uint emissive_texture_index = scene_instance.texture_indices_and_texcoords.z;
    const float3 sampled_emissive = SampleMaterialTextureRGBA(
        emissive_texture_index,
        material_uv,
        float4(1.0f, 1.0f, 1.0f, 1.0f)).rgb;
    return max(sampled_emissive * emissive_factor, 0.0f);
}

float3 SampleNormalTextureTS(BakeSceneInstance scene_instance, float2 material_uv)
{
    const uint normal_texture_index = scene_instance.texture_indices_and_texcoords_extra.x;
    const float normal_scale = max(scene_instance.metallic_alpha_normal_and_padding.z, 0.0f);
    const float3 sampled_normal = SampleMaterialTextureRGBA(
        normal_texture_index,
        material_uv,
        float4(0.5f, 0.5f, 1.0f, 1.0f)).xyz;
    float3 tangent_space_normal = sampled_normal * 2.0f - 1.0f;
    tangent_space_normal.xy *= normal_scale;
    return SafeNormalize(tangent_space_normal, float3(0.0f, 0.0f, 1.0f));
}

// glTF stores roughness in G and metallic in B.
float2 SampleMetallicRoughness(BakeSceneInstance scene_instance, float2 material_uv)
{
    const uint metallic_roughness_texture_index = scene_instance.texture_indices_and_texcoords_extra.z;
    const float metallic_factor = saturate(scene_instance.metallic_alpha_normal_and_padding.x);
    const float roughness_factor = saturate(scene_instance.emissive_and_roughness.w);
    if (metallic_roughness_texture_index == 0xffffffffu)
    {
        return float2(metallic_factor, roughness_factor);
    }

    const float2 sampled_metallic_roughness = SampleMaterialTextureRGBA(
        metallic_roughness_texture_index,
        material_uv,
        float4(1.0f, 1.0f, 1.0f, 1.0f)).bg;
    return saturate(sampled_metallic_roughness * float2(metallic_factor, roughness_factor));
}

void BuildShadingBasisFromTangent(float3 shading_normal, float4 interpolated_tangent, out float3 tangent, out float3 bitangent)
{
    tangent = interpolated_tangent.xyz - shading_normal * dot(interpolated_tangent.xyz, shading_normal);
    const float tangent_length_sq = dot(tangent, tangent);
    if (tangent_length_sq <= 1e-8f)
    {
        BuildOrthonormalBasis(shading_normal, tangent, bitangent);
        return;
    }

    tangent *= rsqrt(tangent_length_sq);
    const float handedness = interpolated_tangent.w >= 0.0f ? 1.0f : -1.0f;
    bitangent = SafeNormalize(cross(shading_normal, tangent) * handedness, float3(0.0f, 1.0f, 0.0f));
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
    if (texel_record.world_position_and_valid.w < 0.5f || sample_count == 0u)
    {
        g_bake_output[launch_index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float3 texel_normal = texel_record.geometric_normal_and_padding.xyz;
    texel_normal = SafeNormalize(texel_normal, float3(0.0f, 1.0f, 0.0f));

    float3 texel_tangent;
    float3 texel_bitangent;
    BuildOrthonormalBasis(texel_normal, texel_tangent, texel_bitangent);

    float3 radiance_sum = float3(0.0f, 0.0f, 0.0f);
    for (uint sample_offset = 0u; sample_offset < sample_count; ++sample_offset)
    {
        const uint current_sample_index = sample_index + sample_offset;
        uint rng_state = Hash32(pixel_index ^ (current_sample_index * 0xA511E9B3u) ^ (max_bounces * 0x63D83595u));

        const float2 texel_sample_uv = float2(Random01(rng_state), Random01(rng_state));
        const float3 texel_local_direction = SampleCosineHemisphere(texel_sample_uv);

        float3 current_origin = texel_record.world_position_and_valid.xyz + texel_normal * ray_epsilon;
        float3 current_direction = SafeNormalize(
            texel_local_direction.x * texel_tangent +
            texel_local_direction.y * texel_bitangent +
            texel_local_direction.z * texel_normal,
            texel_normal);

        float3 throughput = float3(1.0f, 1.0f, 1.0f);
        float3 sample_radiance = float3(0.0f, 0.0f, 0.0f);
        float pending_environment_miss_weight = 1.0f;
        float pending_emissive_hit_bsdf_pdf = 0.0f;

        for (uint bounce_index = 0u; bounce_index < max_bounces; ++bounce_index)
        {
            RayDesc ray_desc;
            ray_desc.Origin = current_origin;
            ray_desc.Direction = current_direction;
            ray_desc.TMin = ray_epsilon;
            ray_desc.TMax = 10000.0f;

            BakePayload payload;
            payload.radiance_and_hit = kTracePayloadSentinel;
            payload.normal_and_distance = float4(0.0f, 1.0f, 0.0f, 0.0f);
            payload.albedo_and_padding = float4(0.0f, 0.0f, 0.0f, 0.0f);
            payload.sampling_pdf_and_flags = float4(0.0f, 0.0f, 0.0f, 0.0f);

            TraceRay(
                g_scene_as,
                RAY_FLAG_NONE,
                0xff,
                0,
                1,
                0,
                ray_desc,
                payload);

            if (payload.radiance_and_hit.a < 0.5f)
            {
                sample_radiance += throughput * payload.radiance_and_hit.rgb * pending_environment_miss_weight;
                break;
            }

            const bool hit_is_emissive = any(payload.radiance_and_hit.rgb > 1e-6f);
            float emissive_hit_mis_weight = 1.0f;
            if (hit_is_emissive && pending_emissive_hit_bsdf_pdf > 0.0f)
            {
                const float emissive_direct_pdf = max(payload.sampling_pdf_and_flags.x, 0.0f);
                if (emissive_direct_pdf > 0.0f)
                {
                    emissive_hit_mis_weight =
                        PowerHeuristic(pending_emissive_hit_bsdf_pdf, (float)direct_light_sample_count * emissive_direct_pdf);
                }
            }

            sample_radiance += throughput * payload.radiance_and_hit.rgb * emissive_hit_mis_weight;
            const float hit_distance = payload.normal_and_distance.w;
            if (hit_distance <= ray_epsilon)
            {
                break;
            }

            const float3 hit_position = current_origin + current_direction * hit_distance;
            const float3 hit_normal = SafeNormalize(payload.normal_and_distance.xyz, -current_direction);
            const float3 albedo = max(payload.albedo_and_padding.rgb, 0.0f);
            float3 next_throughput = throughput * albedo;
            const float throughput_max = max(next_throughput.r, max(next_throughput.g, next_throughput.b));
            bool continue_path = throughput_max > 1e-4f && bounce_index + 1u < max_bounces;
            if (continue_path && bounce_index >= 2u)
            {
                const float rr_probability = clamp(throughput_max, 0.1f, 0.95f);
                if (Random01(rng_state) > rr_probability)
                {
                    continue_path = false;
                }
                else
                {
                    next_throughput /= rr_probability;
                }
            }

            sample_radiance += throughput * EvaluateDirectLighting(hit_position, hit_normal, albedo, rng_state);
            sample_radiance += throughput * EvaluateEmissiveDirectLighting(
                hit_position,
                hit_normal,
                albedo,
                hit_is_emissive,
                continue_path,
                rng_state);

            float3 hit_tangent;
            float3 hit_bitangent;
            BuildOrthonormalBasis(hit_normal, hit_tangent, hit_bitangent);

            if (environment_light_sample_count > 0u)
            {
                sample_radiance += throughput * EvaluateEnvironmentLighting(
                    hit_position,
                    hit_normal,
                    albedo,
                    environment_light_sample_count,
                    continue_path,
                    rng_state);
            }

            if (!continue_path)
            {
                break;
            }

            const float2 bounce_sample_uv = float2(Random01(rng_state), Random01(rng_state));
            const float3 bounce_local_direction = SampleCosineHemisphere(bounce_sample_uv);
            const float3 next_direction = SafeNormalize(
                bounce_local_direction.x * hit_tangent +
                bounce_local_direction.y * hit_bitangent +
                bounce_local_direction.z * hit_normal,
                hit_normal);
            const float next_bsdf_pdf = saturate(dot(hit_normal, next_direction)) * kInvPi;
            throughput = next_throughput;
            current_origin = hit_position + hit_normal * ray_epsilon;
            current_direction = next_direction;
            pending_environment_miss_weight = 1.0f;
            pending_emissive_hit_bsdf_pdf = 0.0f;
            if (!hit_is_emissive && direct_light_sample_count > 0u && emissive_triangle_count > 0u)
            {
                pending_emissive_hit_bsdf_pdf = next_bsdf_pdf;
            }
            if (environment_light_sample_count > 0u && sky_intensity > 0.0f)
            {
                const float environment_pdf = EvaluateEnvironmentImportancePdf(next_direction);
                if (environment_pdf > 0.0f)
                {
                    pending_environment_miss_weight =
                        PowerHeuristic(next_bsdf_pdf, (float)environment_light_sample_count * environment_pdf);
                }
            }
        }

        radiance_sum += sample_radiance;
    }

    g_bake_output[launch_index] = float4(radiance_sum / max((float)sample_count, 1.0f), 1.0f);
}

[shader("miss")]
void BakeMissMain(inout BakePayload payload)
{
    payload.radiance_and_hit = float4(EvaluateSkyRadiance(SafeNormalize(WorldRayDirection(), float3(0.0f, 1.0f, 0.0f))), 0.0f);
    payload.normal_and_distance = float4(0.0f, 1.0f, 0.0f, 0.0f);
    payload.albedo_and_padding = float4(0.0f, 0.0f, 0.0f, 0.0f);
    payload.sampling_pdf_and_flags = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void BakeClosestHitMain(inout BakePayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    const uint instance_id = InstanceID();
    const BakeSceneInstance scene_instance = g_scene_instances[instance_id];
    const uint3 triangle_indices = LoadTriangleVertexIndices(instance_id, PrimitiveIndex());

    const BakeSceneVertex vertex0 = g_scene_vertices[triangle_indices.x];
    const BakeSceneVertex vertex1 = g_scene_vertices[triangle_indices.y];
    const BakeSceneVertex vertex2 = g_scene_vertices[triangle_indices.z];

    const float3 barycentrics = float3(
        1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x,
        attributes.barycentrics.y);

    const float3 world_position = InterpolateFloat3(
        vertex0.world_position.xyz,
        vertex1.world_position.xyz,
        vertex2.world_position.xyz,
        barycentrics);

    const float3 interpolated_normal = InterpolateFloat3(
        vertex0.world_normal.xyz,
        vertex1.world_normal.xyz,
        vertex2.world_normal.xyz,
        barycentrics);
    const float4 interpolated_tangent =
        vertex0.world_tangent * barycentrics.x +
        vertex1.world_tangent * barycentrics.y +
        vertex2.world_tangent * barycentrics.z;
    const float2 material_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords.y);
    const float2 normal_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords_extra.y);
    const float2 emissive_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords.w);
    const float2 metallic_roughness_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords_extra.w);

    const float3 edge01 = vertex1.world_position.xyz - vertex0.world_position.xyz;
    const float3 edge02 = vertex2.world_position.xyz - vertex0.world_position.xyz;
    float3 geometric_normal = SafeNormalize(cross(edge01, edge02), float3(0.0f, 1.0f, 0.0f));

    const bool front_face = dot(WorldRayDirection(), geometric_normal) < 0.0f;
    const bool double_sided = (scene_instance.offsets_and_flags.w & kInstanceFlagDoubleSided) != 0u;
    if (!front_face)
    {
        geometric_normal = -geometric_normal;
    }

    float3 shading_normal = SafeNormalize(interpolated_normal, geometric_normal);
    if (dot(shading_normal, geometric_normal) < 0.0f)
    {
        shading_normal = geometric_normal;
    }
    if (dot(shading_normal, WorldRayDirection()) > 0.0f)
    {
        shading_normal = -shading_normal;
    }

    const uint normal_texture_index = scene_instance.texture_indices_and_texcoords_extra.x;
    if (normal_texture_index != 0xffffffffu)
    {
        float3 tangent;
        float3 bitangent;
        BuildShadingBasisFromTangent(shading_normal, interpolated_tangent, tangent, bitangent);
        const float3 tangent_space_normal = SampleNormalTextureTS(scene_instance, normal_uv);
        const float3 mapped_normal =
            tangent * tangent_space_normal.x +
            bitangent * tangent_space_normal.y +
            shading_normal * tangent_space_normal.z;
        shading_normal = SafeNormalize(mapped_normal, shading_normal);
        if (dot(shading_normal, geometric_normal) < 0.0f)
        {
            shading_normal = geometric_normal;
        }
        if (dot(shading_normal, WorldRayDirection()) > 0.0f)
        {
            shading_normal = -shading_normal;
        }
    }

    const float3 base_color = SampleBaseColor(scene_instance, material_uv);
    const float3 emissive = SampleEmissive(scene_instance, emissive_uv);
    const float2 metallic_roughness = SampleMetallicRoughness(scene_instance, metallic_roughness_uv);
    const float metallic = saturate(metallic_roughness.x);
    const float roughness = saturate(metallic_roughness.y);
    const float3 diffuse_albedo = base_color * (1.0f - metallic);
    const float3 visible_albedo = (!front_face && !double_sided) ? float3(0.0f, 0.0f, 0.0f) : diffuse_albedo;

    payload.radiance_and_hit = float4(emissive, 1.0f);
    payload.normal_and_distance = float4(shading_normal, RayTCurrent());
    payload.albedo_and_padding = float4(visible_albedo, roughness);
    payload.sampling_pdf_and_flags = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (any(emissive > 1e-6f))
    {
        float triangle_area = 0.0f;
        float selection_pdf = 0.0f;
        if (TryGetEmissiveTriangleSamplingData(instance_id, PrimitiveIndex(), triangle_area, selection_pdf))
        {
            const float light_cosine = abs(dot(geometric_normal, -WorldRayDirection()));
            payload.sampling_pdf_and_flags.x =
                EvaluateEmissiveTriangleDirectPdf(triangle_area, selection_pdf, RayTCurrent(), light_cosine);
        }
    }
}

[shader("anyhit")]
void BakeAnyHitMain(inout BakePayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    const uint instance_id = InstanceID();
    const BakeSceneInstance scene_instance = g_scene_instances[instance_id];
    if ((scene_instance.offsets_and_flags.w & kInstanceFlagAlphaMasked) == 0u)
    {
        return;
    }

    const uint3 triangle_indices = LoadTriangleVertexIndices(instance_id, PrimitiveIndex());
    const BakeSceneVertex vertex0 = g_scene_vertices[triangle_indices.x];
    const BakeSceneVertex vertex1 = g_scene_vertices[triangle_indices.y];
    const BakeSceneVertex vertex2 = g_scene_vertices[triangle_indices.z];

    const float3 barycentrics = float3(
        1.0f - attributes.barycentrics.x - attributes.barycentrics.y,
        attributes.barycentrics.x,
        attributes.barycentrics.y);
    const float2 material_uv = SelectMaterialTexcoord(
        vertex0,
        vertex1,
        vertex2,
        barycentrics,
        scene_instance.texture_indices_and_texcoords.y);
    const float surface_alpha = SampleBaseAlpha(scene_instance, material_uv);
    const float alpha_cutoff = saturate(scene_instance.metallic_alpha_normal_and_padding.y);
    if (surface_alpha < alpha_cutoff)
    {
        IgnoreHit();
    }
    (void)payload;
}

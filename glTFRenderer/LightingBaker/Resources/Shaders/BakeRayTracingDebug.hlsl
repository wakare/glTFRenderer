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
    float4 texcoord0_texcoord1;
};

struct BakeSceneInstance
{
    uint4 offsets_and_flags;
    uint4 texture_indices_and_texcoords;
    float4 base_color;
    float4 emissive_and_roughness;
    float4 metallic_and_padding;
};

struct BakeSceneLight
{
    float4 position_and_type;
    float4 direction_and_range;
    float4 color_and_intensity;
    float4 spot_angles;
};

StructuredBuffer<BakeTexelRecord> g_bake_texel_records : register(t1);
StructuredBuffer<BakeSceneVertex> g_scene_vertices : register(t2);
StructuredBuffer<uint> g_scene_indices : register(t3);
StructuredBuffer<BakeSceneInstance> g_scene_instances : register(t4);
StructuredBuffer<BakeSceneLight> g_scene_lights : register(t5);
Texture2D<float4> bindless_bake_material_textures[] : register(t0, BINDLESS_TEXTURE_SPACE_BAKER_MATERIAL);
RWTexture2D<float4> g_bake_output : register(u0);

cbuffer g_bake_dispatch_constants : register(b0)
{
    uint atlas_width;
    uint atlas_height;
    uint sample_index;
    uint sample_count;
    uint max_bounces;
    uint scene_light_count;
    float ray_epsilon;
    float sky_intensity;
    float sky_ground_mix;
    float padding0;
    float padding1;
    float padding2;
};

struct [raypayload] BakePayload
{
    float4 radiance_and_hit : read(caller) : write(caller, miss, closesthit);
    float4 normal_and_distance : read(caller) : write(caller, closesthit, miss);
    float4 albedo_and_padding : read(caller) : write(caller, closesthit, miss);
};

static const float kPi = 3.14159265359f;
static const uint kInstanceFlagDoubleSided = 1u;
static const uint kInstanceFlagAlphaMasked = 1u << 1u;
static const uint kMaterialTextureInvalidIndex = 0xffffffffu;
static const uint kSceneLightTypeDirectional = 0u;
static const uint kSceneLightTypePoint = 1u;
static const uint kSceneLightTypeSpot = 2u;
static const float4 kTracePayloadSentinel = float4(-1.0f, -2.0f, -3.0f, -4.0f);

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

float3 EvaluateDirectLighting(float3 hit_position, float3 hit_normal, float3 albedo)
{
    if (scene_light_count == 0u)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 lambert_brdf = albedo * rcp(kPi);
    float3 direct_radiance = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (uint light_index = 0u; light_index < scene_light_count; ++light_index)
    {
        const BakeSceneLight light = g_scene_lights[light_index];
        const uint light_type = (uint)(light.position_and_type.w + 0.5f);
        const float3 light_color = max(light.color_and_intensity.rgb, 0.0f);
        const float light_intensity = max(light.color_and_intensity.w, 0.0f);
        if (light_intensity <= 0.0f || all(light_color <= 0.0f))
        {
            continue;
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
                continue;
            }

            const float distance_to_light = sqrt(distance_sq);
            light_direction = to_light / distance_to_light;
            shadow_distance = max(distance_to_light - ray_epsilon, ray_epsilon);
            light_attenuation = EvaluateRangeAttenuation(distance_sq, light.direction_and_range.w);
            if (light_attenuation <= 0.0f)
            {
                continue;
            }

            if (light_type == kSceneLightTypeSpot)
            {
                const float spot_attenuation = EvaluateSpotAttenuation(light, -light_direction);
                if (spot_attenuation <= 0.0f)
                {
                    continue;
                }

                light_attenuation *= spot_attenuation;
            }
        }

        const float n_dot_l = saturate(dot(hit_normal, light_direction));
        if (n_dot_l <= 0.0f)
        {
            continue;
        }

        if (!TraceShadowVisibility(hit_position + hit_normal * ray_epsilon, light_direction, shadow_distance))
        {
            continue;
        }

        const float3 incident_radiance = light_color * (light_intensity * light_attenuation);
        direct_radiance += lambert_brdf * incident_radiance * n_dot_l;
    }

    return direct_radiance;
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

            TraceRay(
                g_scene_as,
                RAY_FLAG_NONE,
                0xff,
                0,
                1,
                0,
                ray_desc,
                payload);

            sample_radiance += throughput * payload.radiance_and_hit.rgb;
            if (payload.radiance_and_hit.a < 0.5f)
            {
                break;
            }

            const float hit_distance = payload.normal_and_distance.w;
            if (hit_distance <= ray_epsilon)
            {
                break;
            }

            const float3 hit_position = current_origin + current_direction * hit_distance;
            const float3 hit_normal = SafeNormalize(payload.normal_and_distance.xyz, -current_direction);
            const float3 albedo = max(payload.albedo_and_padding.rgb, 0.0f);
            sample_radiance += throughput * EvaluateDirectLighting(hit_position, hit_normal, albedo);
            throughput *= albedo;
            const float throughput_max = max(throughput.r, max(throughput.g, throughput.b));
            if (throughput_max <= 1e-4f || bounce_index + 1u >= max_bounces)
            {
                break;
            }

            if (bounce_index >= 2u)
            {
                const float rr_probability = clamp(throughput_max, 0.1f, 0.95f);
                if (Random01(rng_state) > rr_probability)
                {
                    break;
                }

                throughput /= rr_probability;
            }

            float3 hit_tangent;
            float3 hit_bitangent;
            BuildOrthonormalBasis(hit_normal, hit_tangent, hit_bitangent);

            const float2 bounce_sample_uv = float2(Random01(rng_state), Random01(rng_state));
            const float3 bounce_local_direction = SampleCosineHemisphere(bounce_sample_uv);
            current_origin = hit_position + hit_normal * ray_epsilon;
            current_direction = SafeNormalize(
                bounce_local_direction.x * hit_tangent +
                bounce_local_direction.y * hit_bitangent +
                bounce_local_direction.z * hit_normal,
                hit_normal);
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
    const float2 material_uv = SelectMaterialTexcoord(
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

    const float3 base_color = SampleBaseColor(scene_instance, material_uv);
    const float3 emissive = SampleEmissive(scene_instance, emissive_uv);
    const float3 visible_albedo = (!front_face && !double_sided) ? float3(0.0f, 0.0f, 0.0f) : base_color;

    payload.radiance_and_hit = float4(emissive, 1.0f);
    payload.normal_and_distance = float4(shading_normal, RayTCurrent());
    payload.albedo_and_padding = float4(visible_albedo, world_position.x * 0.0f);
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
    const float alpha_cutoff = saturate(scene_instance.metallic_and_padding.y);
    if (surface_alpha < alpha_cutoff)
    {
        IgnoreHit();
    }
    (void)payload;
}

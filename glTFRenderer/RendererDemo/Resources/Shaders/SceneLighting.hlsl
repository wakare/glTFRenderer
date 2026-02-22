
#include "SceneViewCommon.hlsl"
#include "RendererModule/RendererModuleLighting.hlsl"

Texture2D albedoTex;
Texture2D normalTex;
Texture2D depthTex;

RWTexture2D<float4> Output;

float3 GetWorldPosition(int2 texCoord)
{
    float depth = depthTex.Load(int3(texCoord, 0)).r;

    float2 uv = texCoord / float2(viewport_width - 1, viewport_height - 1);
    
    // Flip uv.y is important
    float4 clipSpaceCoord = float4(2 * uv.x - 1.0, 1 - 2 * uv.y, depth, 1.0);
    float4 worldSpaceCoord = mul(inverse_view_projection_matrix, clipSpaceCoord);
    worldSpaceCoord /= worldSpaceCoord.w;

    return worldSpaceCoord.xyz;
}

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= viewport_width || dispatchThreadID.y >= viewport_height)
    {
        return;
    }

    float3 world_position = GetWorldPosition(dispatchThreadID.xy);

    float4 albedo_buffer_data = albedoTex.Load(int3(dispatchThreadID.xy, 0));
    float4 normal_buffer_data = normalTex.Load(int3(dispatchThreadID.xy, 0));
    
    float3 albedo = albedo_buffer_data.xyz;
    float metallic = albedo_buffer_data.w;
    
    float3 normal = normalize(2 * normal_buffer_data.xyz - 1);
    float roughness = normal_buffer_data.w;

    {
        PixelLightingShadingInfo shading_info;
        shading_info.albedo = albedo;
        shading_info.position = world_position;
        shading_info.normal = normal;
        shading_info.metallic = metallic;
        shading_info.roughness = roughness;
        shading_info.backface = false;
    
        float3 view = normalize(view_position.xyz - world_position);
    
        float3 final_lighting = GetLighting(shading_info, view);
    
        Output[dispatchThreadID.xy] = float4(LinearToSrgb(final_lighting), 1.0);    
    }
}

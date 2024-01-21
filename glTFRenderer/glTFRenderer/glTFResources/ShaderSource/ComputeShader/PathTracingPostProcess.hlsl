#ifndef PATH_TRACING_POST_PROCESS
#define PATH_TRACING_POST_PROCESS
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"

Texture2D<float4> postprocess_input_texture : POST_PROCESS_INPUT_REGISTER_INDEX;
Texture2D<float4> screen_uv_offset : SCREEN_UV_OFFSET_REGISTER_INDEX;

RWTexture2D<float4> accumulation_output : ACCUMULATION_OUTPUT_REGISTER_INDEX;
Texture2D<float4> accumulation_back_buffer : ACCUMULATION_BACKBUFFER_REGISTER_INDEX;

RWTexture2D<float4> custom_output : CUSTOM_OUTPUT_REGISTER_INDEX;
Texture2D<float4> custom_back_buffer : CUSTOM_BACKBUFFER_REGISTER_INDEX;

RWTexture2D<float4> postprocess_output : POST_PROCESS_OUTPUT_REGISTER_INDEX;

cbuffer RayTracingPostProcessPassOptions: RAY_TRACING_POSTPROCESS_OPTION_CBV_INDEX
{
    bool enable_post_process;
    bool use_velocity_clamp;
    float reuse_history_factor;
    int color_clamp_range;
};

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= viewport_width ||
        dispatchThreadID.y >= viewport_height)
    {
        return;
    }
    
    float4 postprocess_input = postprocess_input_texture.Load(int3(dispatchThreadID.xy, 0));
    if (!enable_post_process)
    {
        postprocess_output[dispatchThreadID.xy] = postprocess_input;
        return;
    }
    
    float2 prev_screen_uv = screen_uv_offset.Load(int3(dispatchThreadID.xy, 0)).xy;
    int2 prev_screen_coord = int2(round(prev_screen_uv.x * viewport_width), round(prev_screen_uv.y * viewport_height));
    bool reuse_history = prev_screen_uv.x >= 0.0 && prev_screen_uv.x <= 1.0 &&
        prev_screen_uv.y >= 0.0 && prev_screen_uv.y <= 1.0;
    float4 accumulation_result = accumulation_back_buffer[prev_screen_coord];
    float velocity_clamp_factor = 1.0;
    if (use_velocity_clamp)
    {
        float2 current_screen_uv = dispatchThreadID.xy / float2(viewport_width, viewport_height);
        float2 current_screen_uv_offset = current_screen_uv - prev_screen_uv;
        float2 prev_screen_uv_offset = prev_screen_uv - screen_uv_offset.Load(int3(prev_screen_coord, 0)).xy;
        velocity_clamp_factor = 1.0 - saturate(100.0 * (length(current_screen_uv_offset - prev_screen_uv_offset) - 0.0001));
    }
    
    if (accumulation_result.w > 1.0)
    {
        // Color clamping
        float3 min_color = 1.0;
        float3 max_color = 0.0;
        
        float3 clamped_history_color = accumulation_result.xyz / accumulation_result.w;
        for (int x = -color_clamp_range; x <= color_clamp_range; ++x)
        {
            for (int y = -color_clamp_range; y <= color_clamp_range; ++y)
            {
                int2 neighborhood_coord = max(prev_screen_coord + int2(x, y), 0.0);
                float4 color = postprocess_input_texture.Load(int3(neighborhood_coord, 0));
                float3 normalized_color = color.xyz / max(color.w, 0.001);
                min_color = min(min_color, normalized_color);
                max_color = max(max_color, normalized_color);
            }
        }
        clamped_history_color = clamp(clamped_history_color, min_color, max_color);
        accumulation_output[dispatchThreadID.xy] = float4(accumulation_result.w * clamped_history_color, accumulation_result.w);
    }

    accumulation_output[dispatchThreadID.xy] = reuse_history ? reuse_history_factor * accumulation_result * velocity_clamp_factor : 0.0;
    accumulation_output[dispatchThreadID.xy] += float4(postprocess_input.xyz, 1.0);
    
    float3 final_color = accumulation_output[dispatchThreadID.xy].xyz / accumulation_output[dispatchThreadID.xy].w;
    postprocess_output[dispatchThreadID.xy] = float4(final_color, 1.0);
}

#endif
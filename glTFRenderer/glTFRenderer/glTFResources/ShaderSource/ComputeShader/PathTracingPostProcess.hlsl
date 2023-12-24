#ifndef PATH_TRACING_POST_PROCESS
#define PATH_TRACING_POST_PROCESS
#include "glTFResources/ShaderSource/Interface/SceneView.hlsl"

Texture2D<float4> postprocess_input_texture : POST_PROCESS_INPUT_REGISTER_INDEX;
Texture2D<float4> screen_uv_offset : SCREEN_UV_OFFSET_REGISTER_INDEX;

RWTexture2D<float4> accumulation_output : ACCUMULATION_OUTPUT_REGISTER_INDEX;
Texture2D<float4> accumulation_backupbuffer : ACCUMULATION_BACKBUFFER_REGISTER_INDEX;

RWTexture2D<float4> custom_output : CUSTOM_OUTPUT_REGISTER_INDEX;
Texture2D<float4> custom_backupbuffer : CUSTOM_BACKBUFFER_REGISTER_INDEX;

RWTexture2D<float4> postprocess_output : POST_PROCESS_OUTPUT_REGISTER_INDEX;

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= viewport_width ||
        dispatchThreadID.y >= viewport_height)
    {
        return;
    }
    
    float4 postprocess_input = postprocess_input_texture.Load(int3(dispatchThreadID.xy, 0));
    float2 prev_screen_position = screen_uv_offset.Load(int3(dispatchThreadID.xy, 0)).xy;
    bool reuse_history = false;
      
    if (!reuse_history)
    {
        accumulation_output[dispatchThreadID.xy] = 0.0;
    }
        
    accumulation_output[dispatchThreadID.xy] += float4(postprocess_input.xyz, 1.0);
    float3 final_color = accumulation_output[dispatchThreadID.xy].xyz / accumulation_output[dispatchThreadID.xy].w;
    postprocess_output[dispatchThreadID.xy] = float4(final_color, 1.0);
}

#endif
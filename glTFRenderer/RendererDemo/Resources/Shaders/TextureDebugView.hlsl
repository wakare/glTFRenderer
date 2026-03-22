Texture2D<float4> InputColorTex;
Texture2D<float> InputScalarTex;
RWTexture2D<float4> Output;

cbuffer TextureDebugViewGlobalBuffer
{
    float exposure;
    float gamma;
    uint tone_map_mode;
    uint visualization_mode;
    float scale;
    float bias;
    uint apply_tonemap;
    float pad0;
};

float3 ToneMapReinhard(float3 color)
{
    return color / (1.0f + color);
}

float3 ToneMapACES(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 ApplyToneMapPresentation(float3 color)
{
    float3 mapped = max(color * exposure, 0.0f);
    if (tone_map_mode == 0)
    {
        mapped = ToneMapReinhard(mapped);
    }
    else
    {
        mapped = ToneMapACES(mapped);
    }

    const float inv_gamma = 1.0f / max(gamma, 1.0e-4f);
    return pow(saturate(mapped), inv_gamma);
}

[numthreads(8, 8, 1)]
void MainColor(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint width = 0;
    uint height = 0;
    InputColorTex.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float4 sampled = InputColorTex.Load(int3(pixel, 0));
    if (visualization_mode == 2u)
    {
        const float2 velocity = sampled.xy * scale;
        const float speed = saturate(length(velocity));
        const float3 velocity_color = saturate(float3(
            0.5f + 0.5f * velocity.x,
            0.5f - 0.5f * velocity.y,
            speed));
        Output[pixel] = float4(velocity_color, 1.0f);
        return;
    }

    float3 color = sampled.rgb * scale + bias;
    if (apply_tonemap != 0u)
    {
        color = ApplyToneMapPresentation(color);
    }
    else
    {
        color = saturate(color);
    }

    Output[pixel] = float4(color, 1.0f);
}

[numthreads(8, 8, 1)]
void MainScalar(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint width = 0;
    uint height = 0;
    InputScalarTex.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    const float value = saturate(InputScalarTex.Load(int3(pixel, 0)) * scale + bias);
    Output[pixel] = float4(value.xxx, 1.0f);
}

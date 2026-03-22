Texture2D<float4> InputColorTex;
RWTexture2D<float4> Output;

cbuffer ToneMapGlobalBuffer
{
    float exposure;
    float gamma;
    uint tone_map_mode;
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

[numthreads(8, 8, 1)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint width = 0;
    uint height = 0;
    InputColorTex.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const int2 pixel = dispatch_thread_id.xy;
    float3 color = max(InputColorTex.Load(int3(pixel, 0)).rgb * exposure, 0.0f);

    if (tone_map_mode == 0)
    {
        color = ToneMapReinhard(color);
    }
    else
    {
        color = ToneMapACES(color);
    }

    const float inv_gamma = 1.0f / max(gamma, 1e-4f);
    const float3 srgb_color = pow(saturate(color), inv_gamma);
    Output[pixel] = float4(srgb_color, 1.0f);
}

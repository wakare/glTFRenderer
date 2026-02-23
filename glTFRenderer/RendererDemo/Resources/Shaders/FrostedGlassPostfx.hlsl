Texture2D<float4> InputTex;
RWTexture2D<float4> OutputTex;

cbuffer FrostedGlassGlobalBuffer
{
    uint panel_count;
    uint blur_radius;
    float scene_edge_scale;
    float pad0;
};

int2 ClampToExtent(int2 pixel, int2 extent)
{
    return clamp(pixel, int2(0, 0), max(extent - 1, int2(0, 0)));
}

float4 LoadInputSafe(int2 pixel, int2 extent)
{
    return InputTex.Load(int3(ClampToExtent(pixel, extent), 0));
}

float GaussianWeight(int offset, float sigma)
{
    const float x = (float)offset;
    const float sigma2 = max(sigma * sigma, 1e-5f);
    return exp(-0.5f * x * x / sigma2);
}

float4 Blur1D(int2 pixel, int2 direction, int2 input_extent)
{
    const int radius = max((int)blur_radius, 1);
    const float sigma = max((float)radius * 0.5f, 0.8f);
    float4 accumulated_color = 0.0f;
    float accumulated_weight = 0.0f;

    [loop]
    for (int offset = -radius; offset <= radius; ++offset)
    {
        const float weight = GaussianWeight(offset, sigma);
        accumulated_color += LoadInputSafe(pixel + direction * offset, input_extent) * weight;
        accumulated_weight += weight;
    }

    return accumulated_color / max(accumulated_weight, 1e-5f);
}

[numthreads(8, 8, 1)]
void DownsampleMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint output_width = 0;
    uint output_height = 0;
    OutputTex.GetDimensions(output_width, output_height);
    if (dispatch_thread_id.x >= output_width || dispatch_thread_id.y >= output_height)
    {
        return;
    }

    uint input_width = 0;
    uint input_height = 0;
    InputTex.GetDimensions(input_width, input_height);
    const int2 input_extent = int2((int)max(input_width, 1u), (int)max(input_height, 1u));
    const int2 base_pixel = int2(dispatch_thread_id.xy) * 2;

    const float4 c0 = LoadInputSafe(base_pixel + int2(0, 0), input_extent);
    const float4 c1 = LoadInputSafe(base_pixel + int2(1, 0), input_extent);
    const float4 c2 = LoadInputSafe(base_pixel + int2(0, 1), input_extent);
    const float4 c3 = LoadInputSafe(base_pixel + int2(1, 1), input_extent);
    OutputTex[int2(dispatch_thread_id.xy)] = (c0 + c1 + c2 + c3) * 0.25f;
}

[numthreads(8, 8, 1)]
void BlurHorizontalMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint output_width = 0;
    uint output_height = 0;
    OutputTex.GetDimensions(output_width, output_height);
    if (dispatch_thread_id.x >= output_width || dispatch_thread_id.y >= output_height)
    {
        return;
    }

    uint input_width = 0;
    uint input_height = 0;
    InputTex.GetDimensions(input_width, input_height);
    const int2 input_extent = int2((int)max(input_width, 1u), (int)max(input_height, 1u));
    const int2 pixel = int2(dispatch_thread_id.xy);
    OutputTex[pixel] = Blur1D(pixel, int2(1, 0), input_extent);
}

[numthreads(8, 8, 1)]
void BlurVerticalMain(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint output_width = 0;
    uint output_height = 0;
    OutputTex.GetDimensions(output_width, output_height);
    if (dispatch_thread_id.x >= output_width || dispatch_thread_id.y >= output_height)
    {
        return;
    }

    uint input_width = 0;
    uint input_height = 0;
    InputTex.GetDimensions(input_width, input_height);
    const int2 input_extent = int2((int)max(input_width, 1u), (int)max(input_height, 1u));
    const int2 pixel = int2(dispatch_thread_id.xy);
    OutputTex[pixel] = Blur1D(pixel, int2(0, 1), input_extent);
}

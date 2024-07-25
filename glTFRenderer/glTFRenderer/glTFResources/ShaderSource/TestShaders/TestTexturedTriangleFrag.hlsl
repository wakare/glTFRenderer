struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
};

[[vk::binding(0, 0)]] Texture2D sampled_texture: SAMPLED_TEX_REGISTER_INDEX;
[[vk::binding(0, 1)]] SamplerState default_sampler : DEFAULT_SAMPLER_REGISTER_INDEX;

PSOutput main(VSOutput input)
{
    PSOutput output;
    output.color = sampled_texture.Sample(default_sampler, input.uv);
    return output;
}
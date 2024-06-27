RWTexture2D<float4> Output: register(u0, space0);

struct ConstantBufferData
{
    float4 data0;
    float4 data1;
    float4 data2;
    float4 data3;
};

[[vk::push_constant]]
ConstantBuffer<ConstantBufferData> constant_buffer;

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Output[dispatchThreadID.xy] = float4(constant_buffer.data0.xyz, 1.0);
}
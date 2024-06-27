RWTexture2D<float4> Output: register(u0, space0);

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Output[dispatchThreadID.xy] = float4(1.0, 1.0, 1.0, 1.0);
}
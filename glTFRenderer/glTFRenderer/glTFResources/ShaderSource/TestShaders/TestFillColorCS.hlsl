
#include "glTFResources/ShaderSource/ShaderDeclarationUtil.hlsl"
DECLARE_RESOURCE(RWTexture2D<float4> Output, OUTPUT_TEX_REGISTER_INDEX);

[numthreads(8, 8, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    Output[dispatchThreadID.xy] = float4(1.0, 0.0, 0.0, 1.0);
}
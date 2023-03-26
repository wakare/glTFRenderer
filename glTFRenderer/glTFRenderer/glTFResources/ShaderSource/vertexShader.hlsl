struct VS_INPUT
{
    float3 pos: POSITION;
    float2 texCoord: TEXCOORD;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvpMat;
};


VS_OUTPUT main(VS_INPUT input)
{
    // just pass vertex position straight through
    VS_OUTPUT output;
    output.pos = mul(mvpMat, float4(input.pos, 1.0f));
    output.texCoord = input.texCoord;
    //output.color = input.color;
    return output;
}
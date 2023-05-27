cbuffer ConstantBuffer : register(b0)
{
    float4x4 viewProjectionMat;
    float4x4 inverseViewProjectionMat;
};

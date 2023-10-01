//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

struct RayPayload
{
    float4 color;
    uint   recursionDepth;
};

//RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
typedef BuiltInTriangleIntersectionAttributes MyAttributes;

[shader("raygeneration")]
void MyRaygenShader()
{
    RenderTarget[DispatchRaysIndex().xy] = float4(1.0, 0.0, 0.0, 1.0);
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
}

#endif // RAYTRACING_HLSL
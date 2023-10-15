#ifndef SAMPLE
#define SAMPLE

#include "glTFResources/ShaderSource/Math/MathCommon.hlsl"

// Samples a direction within a hemisphere oriented along +Z axis with a cosine-weighted distribution 
// Source: "Sampling Transformations Zoo" in Ray Tracing Gems by Shirley et al.
float3 sampleHemisphere(float2 u, out float pdf) {

    float a = sqrt(u.x);
    float b = TWO_PI * u.y;

    float3 result = float3(
        a * cos(b),
        a * sin(b),
        sqrt(1.0f - u.x));

    pdf = result.z * ONE_OVER_PI;

    return result;
}

#endif
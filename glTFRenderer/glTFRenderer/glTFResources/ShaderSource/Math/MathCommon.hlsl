#ifndef MATH_COMMON
#define MATH_COMMON

// PIs
#ifndef PI
#define PI 3.141592653589f
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

#ifndef ONE_OVER_PI
#define ONE_OVER_PI (1.0f / PI)
#endif

#ifndef ONE_OVER_TWO_PI
#define ONE_OVER_TWO_PI (1.0f / TWO_PI)
#endif

float3 GetWorldNormal(float3x3 world_matrix, float3 geometry_normal, float4 geometry_tangent, float3 tangent_normal)
{
    float3 tmpTangent = normalize(mul(world_matrix, geometry_tangent.xyz));
    float3 bitangent = cross(geometry_normal, tmpTangent) * geometry_tangent.w;
    float3 tangent = cross(bitangent, geometry_normal);
    float3x3 TBN = transpose(float3x3(tangent, bitangent, geometry_normal));
    return normalize(mul(world_matrix, mul(TBN, tangent_normal)));
}

#endif
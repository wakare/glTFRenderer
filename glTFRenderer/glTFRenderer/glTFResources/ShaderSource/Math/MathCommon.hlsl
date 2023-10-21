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

// Calculates rotation quaternion from input vector to the vector (0, 0, 1)
// Input vector must be normalized!
float4 getRotationToZAxis(float3 input) {

    // Handle special case when input is exact or near opposite of (0, 0, 1)
    if (input.z < -0.99999f) return float4(1.0f, 0.0f, 0.0f, 0.0f);

    return normalize(float4(input.y, -input.x, 0.0f, 1.0f + input.z));
}

// Calculates rotation quaternion from vector (0, 0, 1) to the input vector
// Input vector must be normalized!
float4 getRotationFromZAxis(float3 input) {

    // Handle special case when input is exact or near opposite of (0, 0, 1)
    if (input.z < -0.99999f) return float4(1.0f, 0.0f, 0.0f, 0.0f);

    return normalize(float4(-input.y, input.x, 0.0f, 1.0f + input.z));
}

// Returns the quaternion with inverted rotation
float4 invertRotation(float4 q)
{
    return float4(-q.x, -q.y, -q.z, q.w);
}

// Optimized point rotation using quaternion
// Source: https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
float3 rotatePoint(float4 q, float3 v) {
    const float3 qAxis = float3(q.x, q.y, q.z);
    return 2.0f * dot(qAxis, v) * qAxis + (q.w * q.w - dot(qAxis, qAxis)) * v + 2.0f * q.w * cross(qAxis, v);
}

// Clever offset_ray function from Ray Tracing Gems chapter 6
// Offsets the ray origin from current position p, along normal n (which must be geometric normal)
// so that no self-intersection can occur.
float3 OffsetRay(const float3 p, const float3 n)
{
    static const float origin = 1.0f / 32.0f;
    static const float float_scale = 1.0f / 65536.0f;
    static const float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

    float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

// Conversion between linear and sRGB color spaces
inline float LinearToSrgb(float linearColor)
{
    if (linearColor < 0.0031308f) return linearColor * 12.92f;
    else return 1.055f * float(pow(linearColor, 1.0f / 2.4f)) - 0.055f;
}

inline float srgbToLinear(float srgbColor)
{
    if (srgbColor < 0.04045f) return srgbColor / 12.92f;
    else return float(pow((srgbColor + 0.055f) / 1.055f, 2.4f));
}

inline float3 LinearToSrgb(float3 color)
{
    return float3(LinearToSrgb(color.x),LinearToSrgb(color.y),LinearToSrgb(color.z));
}

inline float3 srgbToLinear(float3 color)
{
    return float3(srgbToLinear(color.x),srgbToLinear(color.y),srgbToLinear(color.z));
}

#endif
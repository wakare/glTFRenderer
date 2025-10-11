#ifndef COLOR
#define COLOR

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

#endif
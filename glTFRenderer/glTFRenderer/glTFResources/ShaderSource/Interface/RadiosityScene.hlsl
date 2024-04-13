#ifndef RADIOSITY_SCENE
#define RADIOSITY_SCENE

struct RadiosityDataOffset
{
    uint offset;
};
StructuredBuffer<RadiosityDataOffset> g_radiosity_data_offset : RadiosityDataOffset_REGISTER_SRV_INDEX;

struct RadiosityFaceInfo
{
    float4 radiosity_irradiance;
};
StructuredBuffer<RadiosityFaceInfo> g_radiosity_face_info : RadiosityFaceInfo_REGISTER_SRV_INDEX;

float3 GetRadiosityFaceInfo(uint instance_id, uint face_id)
{
    uint offset = g_radiosity_data_offset[instance_id].offset + face_id;
    return g_radiosity_face_info[offset].radiosity_irradiance.xyz;
}

#endif
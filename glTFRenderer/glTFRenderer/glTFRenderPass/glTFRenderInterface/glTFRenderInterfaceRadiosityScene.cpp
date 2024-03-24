#include "glTFRenderInterfaceRadiosityScene.h"

#include "glTFRenderInterfaceStructuredBuffer.h"

glTFRenderInterfaceRadiosityScene::glTFRenderInterfaceRadiosityScene()
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<RadiosityDataOffset>>());
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<RadiosityFaceInfo>>());
}

bool glTFRenderInterfaceRadiosityScene::UploadCPUBufferFromRadiosityRenderer(const glTFRadiosityRenderer& radiosity_renderer)
{
    const auto& radiosity_data = radiosity_renderer.GetRadiosityData();

    std::vector<RadiosityDataOffset> data_offsets;
    std::vector<RadiosityFaceInfo> face_infos;

    unsigned current_instance_id = 0;
    data_offsets.push_back({static_cast<unsigned>(face_infos.size())});
    
    for (const auto& data : radiosity_data)
    {
        const MeshInstanceFormFactorData& form_factor_data = data.second;
        unsigned instance_id, face_id;
        DecodeFromFormFactorID(form_factor_data.form_factor_id, instance_id, face_id);

        face_infos.push_back({form_factor_data.irradiance});
        if (current_instance_id != instance_id)
        {
            data_offsets.push_back({static_cast<unsigned>(face_infos.size())});
            current_instance_id = instance_id;
        }
    }

    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<RadiosityDataOffset>>()->UploadCPUBuffer(data_offsets.data(), 0, sizeof(RadiosityDataOffset) * data_offsets.size());
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<RadiosityFaceInfo>>()->UploadCPUBuffer(face_infos.data(), 0, sizeof(RadiosityFaceInfo) * face_infos.size());
    
    return true;
}

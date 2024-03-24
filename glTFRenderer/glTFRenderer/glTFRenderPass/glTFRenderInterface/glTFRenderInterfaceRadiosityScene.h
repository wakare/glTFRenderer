#pragma once

#include "glTFRenderInterfaceBase.h"
#include "glTFApp/glTFRadiosityRenderer.h"

ALIGN_FOR_CBV_STRUCT struct RadiosityDataOffset
{
    inline static std::string Name = "RadiosityDataOffset_REGISTER_SRV_INDEX";
    unsigned offset;
};

ALIGN_FOR_CBV_STRUCT struct RadiosityFaceInfo
{
    inline static std::string Name = "RadiosityFaceInfo_REGISTER_SRV_INDEX";
    glm::vec3 radiosity_irradiance;
};

class glTFRenderInterfaceRadiosityScene : public glTFRenderInterfaceBaseWithDefaultImpl
{
public:
    glTFRenderInterfaceRadiosityScene();
    
    bool UploadCPUBufferFromRadiosityRenderer(const glTFRadiosityRenderer& radiosity_renderer);
};

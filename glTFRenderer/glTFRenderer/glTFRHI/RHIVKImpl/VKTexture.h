#pragma once
#include "glTFRHI/RHIInterface/IRHITexture.h"

class VKTexture : public IRHITexture
{
public:
protected:
    virtual bool InitTexture(IRHIDevice& device, const RHITextureDesc& desc) override;
    virtual bool InitTextureAndUpload(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& desc) override;
};

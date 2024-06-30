#pragma once
#include "glTFRHI/RHIInterface/IRHITexture.h"

class VKTexture : public IRHITexture
{
public:
    virtual IRHIBuffer& GetGPUBuffer() override;

protected:
    virtual bool InitTexture(IRHIDevice& device, IRHICommandList& command_list, const RHITextureDesc& desc) override;
};

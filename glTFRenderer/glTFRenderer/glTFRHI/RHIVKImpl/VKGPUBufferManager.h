#pragma once
#include "glTFRHI/RHIInterface/IRHIGPUBufferManager.h"

class VKGPUBufferManager : public IRHIGPUBufferManager
{
public:
    virtual bool InitGPUBufferManager(IRHIDevice& device, size_t maxDescriptorCount) override;
    virtual bool CreateGPUBuffer(IRHIDevice& device, const RHIBufferDesc& bufferDesc, size_t& outBufferIndex) override;
};

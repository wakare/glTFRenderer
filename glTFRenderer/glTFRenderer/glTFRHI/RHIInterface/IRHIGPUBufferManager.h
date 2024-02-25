#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

// Hold descriptor heap which store cbv for gpu memory
class IRHIGPUBufferManager : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIGPUBufferManager)
    
    virtual bool InitGPUBufferManager(IRHIDevice& device, size_t maxDescriptorCount) = 0;
    virtual bool CreateGPUBuffer(IRHIDevice& device, const RHIBufferDesc& bufferDesc, size_t& outBufferIndex) = 0;
};

#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"

struct RHIDescriptorHeapDesc
{
    unsigned maxDescriptorCount;
    RHIDescriptorHeapType type;
    bool shaderVisible;
};

class IRHIDescriptorHeap: public IRHIResource
{
public:
    virtual bool InitDescriptorHeap(IRHIDevice& device, const RHIDescriptorHeapDesc& desc) = 0;
    virtual RHIGPUDescriptorHandle GetHandle(unsigned offsetInDescriptor) = 0;
};

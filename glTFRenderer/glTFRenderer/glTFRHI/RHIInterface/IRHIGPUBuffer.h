#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIGPUBufferManager;

enum class RHIBufferType
{
    Default,
    Upload,
};

struct RHIBufferDesc
{
    std::wstring name;
    size_t size;
    RHIBufferType type;
};

class IRHIGPUBuffer : public IRHIResource
{
public:
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) = 0;
    virtual bool UploadBufferFromCPU(void* data, size_t size) = 0;

    
};

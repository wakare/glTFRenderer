#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIGPUBufferManager;

enum class RHIBufferResourceType
{
    Buffer,
    Tex1D,
    Tex2D,
    Tex3D,
};

enum class RHIBufferType
{
    Default,
    Upload,
};

enum class RHIBufferUsage
{
    NONE,
    ALLOW_UNORDER_ACCESS,
};

struct RHIBufferDesc
{
    std::wstring name;
    size_t width {0};
    size_t height {0};
    size_t depth {0};
    
    RHIBufferType type ;
    RHIDataFormat resource_data_type;
    RHIBufferResourceType resource_type;
    RHIResourceStateType state {RHIResourceStateType::STATE_COMMON};
    RHIBufferUsage usage {RHIBufferUsage::NONE};
    
    size_t alignment {0};
};

class IRHIGPUBuffer : public IRHIResource
{
public:
    IRHIGPUBuffer();
    
    virtual bool InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc) = 0;
    virtual bool UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetGPUBufferHandle() = 0;

    const RHIBufferDesc& GetBufferDesc() const {return m_buffer_desc; }
    
protected:
    RHIBufferDesc m_buffer_desc;
};
#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

template <typename StructuredBufferType>
class glTFRenderInterfaceStructuredBuffer : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceStructuredBuffer(unsigned structured_buffer_index, unsigned register_index)
        : m_structured_buffer_index(structured_buffer_index)
        , m_register_index(register_index)
    {
    }
    
    virtual bool InitInterface(glTFRenderResourceManager& resource_manager) override
    {
        m_gpu_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        RETURN_IF_FALSE(m_gpu_buffer->InitGPUBuffer(resource_manager.GetDevice(),
            {
                L"StructuredBuffer",
                static_cast<size_t>(64 * 1024),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::R32G32B32A32_FLOAT,
                RHIBufferResourceType::Buffer
            }))
    
        return true;
    }

    virtual bool UpdateCPUBuffer(const void* data, size_t size)
    {
        return m_gpu_buffer->UploadBufferFromCPU(data, 0, size);
    }
    
    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
        RHIUtils::Instance().SetSRVToRootParameterSlot(command_list,
                                                       m_structured_buffer_index, m_gpu_buffer->GetGPUBufferHandle(), isGraphicsPipeline);
    
        return true;
    }

    virtual bool ApplyRootSignature(IRHIRootSignature& rootSignature) const
    {
        return rootSignature.GetRootParameter(m_structured_buffer_index).InitAsSRV(m_register_index);
    }
    
protected:
    unsigned m_structured_buffer_index;
    unsigned m_register_index;
    
    std::shared_ptr<IRHIGPUBuffer> m_gpu_buffer;
};
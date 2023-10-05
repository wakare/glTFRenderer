#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

template <typename StructuredBufferType, size_t max_heap_size = 64ull * 1024>
class glTFRenderInterfaceStructuredBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        m_gpu_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();

        RETURN_IF_FALSE(m_gpu_buffer->InitGPUBuffer(resource_manager.GetDevice(),
            {
                L"StructuredBuffer",
                max_heap_size,
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            }))
    
        return true;
    }

    virtual bool UploadCPUBuffer(const void* data, size_t size) override
    {
        return m_gpu_buffer->UploadBufferFromCPU(data, 0, size);
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
        RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_allocation.parameter_index,
            m_gpu_buffer->GetGPUBufferHandle(), isGraphicsPipeline);
    
        return true;
    }

    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddSRVRootParameter(StructuredBufferType::Name, m_allocation);
    }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        // Update light info shader define
        char registerIndexValue[16] = {'\0'};
    
        (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", GetRSAllocation().register_index);
        out_shader_pre_define_macros.AddMacro(StructuredBufferType::Name, registerIndexValue);
    }
    
protected:
    std::shared_ptr<IRHIGPUBuffer> m_gpu_buffer;
};
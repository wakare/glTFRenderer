#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

template <typename StructuredBufferType>
class glTFRenderInterfaceStructuredBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
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

    virtual bool UpdateCPUBuffer(const void* data, size_t size) override
    {
        return m_gpu_buffer->UploadBufferFromCPU(data, 0, size);
    }
    
    virtual bool ApplyInterface(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
        RHIUtils::Instance().SetSRVToRootParameterSlot(command_list, m_allocation.parameter_index,
            m_gpu_buffer->GetGPUBufferHandle(), isGraphicsPipeline);
    
        return true;
    }

    virtual bool ApplyRootSignature(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddSRVRootParameter(StructuredBufferType::Name, m_allocation);
    }

    virtual void UpdateShaderCompileDefine(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        // Update light info shader define
        char registerIndexValue[16] = {'\0'};
    
        (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", GetRSAllocation().register_index);
        out_shader_pre_define_macros.AddMacro(StructuredBufferType::Name, registerIndexValue);
    }
    
protected:
    std::shared_ptr<IRHIGPUBuffer> m_gpu_buffer;
};
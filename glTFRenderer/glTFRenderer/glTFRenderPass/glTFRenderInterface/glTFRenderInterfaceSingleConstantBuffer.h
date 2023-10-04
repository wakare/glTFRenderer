#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

template <typename ConstantBufferType>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        m_constant_gpu_data = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        // TODO: 64k is enough ??
        RETURN_IF_FALSE(m_constant_gpu_data->InitGPUBuffer(resource_manager.GetDevice(),
            {
                L"GPUBuffer_SingleConstantBuffer",
                static_cast<size_t>(64 * 1024),
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            }))
    
        return true;
    }

    virtual bool UpdateCPUBuffer(const void* data, size_t size) override
    {
        return m_constant_gpu_data->UploadBufferFromCPU(data, 0, size);
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool isGraphicsPipeline) override
    {
        RETURN_IF_FALSE(RHIUtils::Instance().SetCBVToRootParameterSlot(resource_manager.GetCommandListForRecord(),
        m_allocation.parameter_index, m_constant_gpu_data->GetGPUBufferHandle(), isGraphicsPipeline))
    
        return true;
    }

    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddCBVRootParameter("GPUBuffer_SingleConstantBuffer", m_allocation);
    }

    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override
    {
        // Update light info shader define
        char registerIndexValue[16] = {'\0'};
    
        (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", GetRSAllocation().register_index);
        out_shader_pre_define_macros.AddMacro(ConstantBufferType::Name, registerIndexValue);
    }

protected:
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
};
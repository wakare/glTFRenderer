#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

// buffer size must be alignment with 64K
template <typename ConstantBufferType, size_t max_buffer_size = 64ull * 1024>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    glTFRenderInterfaceSingleConstantBuffer()
        : m_current_update_index(0)
    {
    }

    bool UploadAndApplyDataWithIndex(glTFRenderResourceManager& resource_manager, unsigned index, const ConstantBufferType& data, bool is_graphics_pipeline)
    {
        m_current_update_index = index;
        RETURN_IF_FALSE(UploadCPUBuffer(&data, sizeof(data)))
        RETURN_IF_FALSE(ApplyInterfaceImpl(resource_manager, is_graphics_pipeline))
        m_current_update_index = 0;

        return true;
    }
    
    virtual bool UploadCPUBuffer(const void* data, size_t size) override
    {
        return m_constant_gpu_data->UploadBufferFromCPU(data, GetCurrentUpdateDataOffsetWithAlignment(), size);
    }

protected:
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        m_constant_gpu_data = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
        RETURN_IF_FALSE(m_constant_gpu_data->InitGPUBuffer(resource_manager.GetDevice(),
            {
                L"GPUBuffer_SingleConstantBuffer",
                max_buffer_size,
                1,
                1,
                RHIBufferType::Upload,
                RHIDataFormat::Unknown,
                RHIBufferResourceType::Buffer
            }))
    
        return true;
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool is_graphics_pipeline) override
    {
        const RHIGPUDescriptorHandle current_update_handle = m_constant_gpu_data->GetGPUBufferHandle() + GetCurrentUpdateDataOffsetWithAlignment();
        RETURN_IF_FALSE(RHIUtils::Instance().SetCBVToRootParameterSlot(resource_manager.GetCommandListForRecord(),
        m_allocation.parameter_index,current_update_handle, is_graphics_pipeline))
    
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

    size_t GetCurrentUpdateDataOffsetWithAlignment() const
    {
        const size_t alignment_size = 256 * (sizeof(ConstantBufferType) / 256 + (sizeof(ConstantBufferType) % 256 ? 1 : 0));
        GLTF_CHECK(m_current_update_index * alignment_size < max_buffer_size);   
        return m_current_update_index * alignment_size;
    }
    
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
    unsigned m_current_update_index;    
};
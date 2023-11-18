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
    {
    }

    bool UploadAndApplyDataWithIndex(glTFRenderResourceManager& resource_manager, unsigned index, const ConstantBufferType& data, bool is_graphics_pipeline)
    {
        RETURN_IF_FALSE(UploadCPUBuffer(&data, GetDataOffsetWithAlignment(index), sizeof(data)))
        RETURN_IF_FALSE(ApplyInterfaceWithOffset(resource_manager, index, is_graphics_pipeline))

        return true;
    }
    
    virtual bool UploadCPUBuffer(const void* data, size_t offset, size_t size) override
    {
        return m_constant_gpu_data->UploadBufferFromCPU(data, GetDataOffsetWithAlignment(offset), size);
    }

    RHIGPUDescriptorHandle GetGPUHandle(unsigned offset) const
    {
        return m_constant_gpu_data->GetGPUBufferHandle() + GetDataOffsetWithAlignment(offset);
    }

    size_t GetDataOffsetWithAlignment(unsigned index) const
    {
        const size_t alignment_size = 256 * (sizeof(ConstantBufferType) / 256 + (sizeof(ConstantBufferType) % 256 ? 1 : 0));
        GLTF_CHECK(index * alignment_size < max_buffer_size);   
        return index * alignment_size;
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
        return ApplyInterfaceWithOffset(resource_manager, 0, is_graphics_pipeline);
    }

    bool ApplyInterfaceWithOffset(glTFRenderResourceManager& resource_manager, unsigned index, bool is_graphics_pipeline) const
    {
        const RHIGPUDescriptorHandle current_update_handle = m_constant_gpu_data->GetGPUBufferHandle() + GetDataOffsetWithAlignment(index);
        RETURN_IF_FALSE(RHIUtils::Instance().SetCBVToRootParameterSlot(resource_manager.GetCommandListForRecord(),
        m_allocation.parameter_index, current_update_handle, is_graphics_pipeline))
    
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
    
    std::shared_ptr<IRHIGPUBuffer> m_constant_gpu_data;
};
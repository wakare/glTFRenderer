#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

// buffer size must be alignment with 64K
template <typename ConstantBufferType, size_t max_buffer_size = 256ull * 1024>
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
        return glTFRenderResourceManager::GetMemoryManager().UploadBufferData(*m_constant_gpu_data, data, GetDataOffsetWithAlignment(offset), size);
    }

    RHIGPUDescriptorHandle GetGPUHandle(unsigned offset) const
    {
        return m_constant_gpu_data->m_buffer->GetGPUBufferHandle() + GetDataOffsetWithAlignment(offset);
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
        glTFRenderResourceManager::GetMemoryManager().AllocateBufferMemory(
        resource_manager.GetDevice(),
        {
            L"GPUBuffer_SingleConstantBuffer",
            max_buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }, m_constant_gpu_data);
        
        return true;
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, bool is_graphics_pipeline) override
    {
        return ApplyInterfaceWithOffset(resource_manager, 0, is_graphics_pipeline);
    }

    bool ApplyInterfaceWithOffset(glTFRenderResourceManager& resource_manager, unsigned index, bool is_graphics_pipeline) const
    {
        const RHIGPUDescriptorHandle current_update_handle = m_constant_gpu_data->m_buffer->GetGPUBufferHandle() + GetDataOffsetWithAlignment(index);
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
        char registerIndexValue[32] = {'\0'};
    
        (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d, space%u)", GetRSAllocation().register_index, GetRSAllocation().space);
        out_shader_pre_define_macros.AddMacro(ConstantBufferType::Name, registerIndexValue);
    }
    
    std::shared_ptr<IRHIBufferAllocation> m_constant_gpu_data;
};
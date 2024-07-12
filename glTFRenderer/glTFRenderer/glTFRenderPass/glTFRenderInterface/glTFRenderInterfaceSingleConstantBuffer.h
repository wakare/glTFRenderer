#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

// buffer size must be alignment with 64K [DX12]
template <typename ConstantBufferType, size_t max_buffer_size = 256ull * 1024>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    glTFRenderInterfaceSingleConstantBuffer()
    {
    }

    virtual bool UploadCPUBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) override
    {
        // TODO: Handle offset with data alignment later
        GLTF_CHECK(offset == 0);
        return resource_manager.GetMemoryManager().UploadBufferData(*m_constant_gpu_data, data, /*GetDataOffsetWithAlignment(offset)*/0, size);
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
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        resource_manager.GetDevice(),
        {
            L"GPUBuffer_SingleConstantBuffer",
            max_buffer_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer
        }, m_constant_gpu_data);
        m_constant_buffer_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIDescriptorAllocation>();
        m_constant_buffer_descriptor_allocation->InitFromBuffer(*m_constant_gpu_data->m_buffer,
            {
                .format = RHIDataFormat::UNKNOWN,
                .dimension = RHIResourceDimension::BUFFER,
                .view_type = RHIViewType::RVT_CBV,
            });
        
        return true;
    }
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override
    {
        descriptor_updater.BindDescriptor(command_list, pipeline_type, m_allocation.parameter_index, *m_constant_buffer_descriptor_allocation);
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
    std::shared_ptr<IRHIDescriptorAllocation> m_constant_buffer_descriptor_allocation;
};
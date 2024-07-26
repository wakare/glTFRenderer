#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

template <typename StructuredBufferType, size_t max_heap_size = 64ull * 1024>
class glTFRenderInterfaceStructuredBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    glTFRenderInterfaceStructuredBuffer(const char* name = StructuredBufferType::Name.c_str())
        : glTFRenderInterfaceWithRSAllocation(name)
    {
        
    }
    
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        resource_manager.GetDevice(),
        {
            L"StructuredBuffer",
            max_heap_size,
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer
        },
        m_gpu_buffer);
        m_constant_buffer_descriptor_allocation = RHIResourceFactory::CreateRHIResource<IRHIBufferDescriptorAllocation>();
        m_constant_buffer_descriptor_allocation->InitFromBuffer(m_gpu_buffer->m_buffer,
            RHIBufferDescriptorDesc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_SRV,
                max_heap_size,
                0
            });
        
        return true;
    }

    virtual bool UploadCPUBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) override
    {
        return resource_manager.GetMemoryManager().UploadBufferData(*m_gpu_buffer, data, 0, size);
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override
    {
        descriptor_updater.BindDescriptor(command_list, pipeline_type, m_allocation.parameter_index, *m_constant_buffer_descriptor_allocation);
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& rootSignature) override
    {
        return rootSignature.AddSRVRootParameter(StructuredBufferType::Name, m_allocation);
    }

protected:
    std::shared_ptr<IRHIBufferAllocation> m_gpu_buffer;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_constant_buffer_descriptor_allocation;
};
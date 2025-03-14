#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

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
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            RUF_ALLOW_SRV,
        },
        m_gpu_buffer);

        RHISRVStructuredBufferDesc desc =
        {
            sizeof(StructuredBufferType),
            max_heap_size / sizeof(StructuredBufferType),
            true,
        };
        
        resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_gpu_buffer->m_buffer,
            RHIBufferDescriptorDesc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_SRV,
                max_heap_size,
                0,
                desc
            }, m_structured_buffer_descriptor_allocation);
        
        return true;
    }

    virtual bool UploadCPUBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) override
    {
        return resource_manager.GetMemoryManager().UploadBufferData(*m_gpu_buffer, data, 0, size);
    }

    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        descriptor_updater.BindTextureDescriptorTable(command_list, pipeline_type,  m_allocation, *m_structured_buffer_descriptor_allocation);
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddSRVRootParameter(StructuredBufferType::Name, m_allocation);
    }

protected:
    std::shared_ptr<IRHIBufferAllocation> m_gpu_buffer;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_structured_buffer_descriptor_allocation;
};
#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"

template <typename StructuredBufferType, RHIViewType type = RHIViewType::RVT_SRV, bool readback = false>
class glTFRenderInterfaceStructuredBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCPUAccessible
{
public:
    glTFRenderInterfaceStructuredBuffer(const char* name = StructuredBufferType::Name.c_str(), size_t buffer_size = 64ull * 1024)
        : glTFRenderInterfaceWithRSAllocation(name)
        , m_buffer_size(buffer_size)
    {
        
    }
    
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override
    {
        resource_manager.GetMemoryManager().AllocateBufferMemory(
        resource_manager.GetDevice(),
        {
            L"StructuredBuffer",
            m_buffer_size,
            1,
            1,
            type == RHIViewType::RVT_SRV ? RHIBufferType::Upload : (readback ? RHIBufferType::Readback : RHIBufferType::Default),
            RHIDataFormat::UNKNOWN,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            static_cast<RHIResourceUsageFlags>((type == RHIViewType::RVT_SRV ? RUF_ALLOW_SRV : RUF_ALLOW_UAV) | RUF_TRANSFER_SRC), 
        },
        m_gpu_buffer);

        if (type == RHIViewType::RVT_SRV)
        {
            RHISRVStructuredBufferDesc srv_structured_buffer_desc =
            {
                sizeof(StructuredBufferType),
                m_buffer_size / sizeof(StructuredBufferType),
                true,
            };

            RHIBufferDescriptorDesc srv_structured_buffer_descriptor_desc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_SRV,
                m_buffer_size,
                0,
                srv_structured_buffer_desc
            };
            
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_gpu_buffer->m_buffer,
               srv_structured_buffer_descriptor_desc, m_structured_buffer_descriptor_allocation);
        }
        else if (type == RHIViewType::RVT_UAV)
        {
            RHIUAVStructuredBufferDesc uav_structured_buffer_desc
            {
            sizeof(StructuredBufferType),
            m_buffer_size / sizeof(StructuredBufferType),
            };
    
            RHIBufferDescriptorDesc uav_structured_buffer_descriptor_desc
            {
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_UAV,
                m_buffer_size,
                0,
                uav_structured_buffer_desc
            };
        
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_gpu_buffer->m_buffer,
                uav_structured_buffer_descriptor_desc, m_structured_buffer_descriptor_allocation);
        }
        
        return true;
    }

    virtual bool UploadBuffer(glTFRenderResourceManager& resource_manager, const void* data, size_t offset, size_t size) override
    {
        return resource_manager.GetMemoryManager().UploadBufferData(*m_gpu_buffer, data, 0, size);
    }

    virtual bool DownloadBuffer(glTFRenderResourceManager& resource_manager, void* data, size_t size) override
    {
        return resource_manager.GetMemoryManager().DownloadBufferData(*m_gpu_buffer, data, size);
    }
    
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override
    {
        descriptor_updater.BindTextureDescriptorTable(command_list, pipeline_type,  m_allocation, *m_structured_buffer_descriptor_allocation);
        return true;
    }
    
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        if (type == RHIViewType::RVT_SRV)
        {
            return root_signature.AddSRVRootParameter(StructuredBufferType::Name, m_allocation);    
        }
        else if (type == RHIViewType::RVT_UAV)
        {
            return root_signature.AddUAVRootParameter(StructuredBufferType::Name, m_allocation);
        }
        return false;
    }

    IRHIBuffer& GetBuffer()
    {
        return *m_gpu_buffer->m_buffer;
    }
    
protected:
    size_t m_buffer_size;
    std::shared_ptr<IRHIBufferAllocation> m_gpu_buffer;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_structured_buffer_descriptor_allocation;
};
#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"

// buffer size must be alignment with 64K [DX12]
template <typename ConstantBufferType, size_t max_buffer_size = 64ull * 1024>
class glTFRenderInterfaceSingleConstantBuffer : public glTFRenderInterfaceWithRSAllocation, public glTFRenderInterfaceCanUploadDataFromCPU
{
public:
    glTFRenderInterfaceSingleConstantBuffer(const char* name = ConstantBufferType::Name.c_str())
        : glTFRenderInterfaceWithRSAllocation(name)
    {}

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
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            RHIResourceUsageFlags::RUF_ALLOW_CBV,
        }, m_constant_gpu_data);
        m_constant_buffer_descriptor_allocation = CreateBufferDescriptor();
        m_constant_buffer_descriptor_allocation->InitFromBuffer(m_constant_gpu_data->m_buffer,
            RHIBufferDescriptorDesc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_CBV,
                max_buffer_size,
                0
            });
        
        return true;
    }
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater) override
    {
        descriptor_updater.BindDescriptor(command_list, pipeline_type, m_allocation, *m_constant_buffer_descriptor_allocation);
        return true;
    }

    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override
    {
        return root_signature.AddCBVRootParameter(ConstantBufferType::Name, m_allocation);
    }

    std::shared_ptr<IRHIBufferAllocation> m_constant_gpu_data;
    std::shared_ptr<IRHIBufferDescriptorAllocation> m_constant_buffer_descriptor_allocation;
};
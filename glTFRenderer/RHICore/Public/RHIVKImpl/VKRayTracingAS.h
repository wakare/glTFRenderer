#pragma once
#include "RHIInterface/IRHIRayTracingAS.h"
#include "VolkUtils.h"

#include <memory>

class IRHIBufferAllocation;
class IRHIAccelerationStructureDescriptorAllocation;

class RHICORE_API VKRayTracingAS : public IRHIRayTracingAS
{
public:
    virtual void SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc) override;
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) override;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const override;

private:
    RHIRayTracingSceneDesc m_scene_desc;
    std::shared_ptr<IRHIAccelerationStructureDescriptorAllocation> m_tlas_descriptor_allocation;
    VkAccelerationStructureKHR m_tlas {VK_NULL_HANDLE};
    std::shared_ptr<IRHIBufferAllocation> m_tlas_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_tlas_scratch_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_instance_buffer;
    std::vector<VkAccelerationStructureKHR> m_blas;
    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_blas_buffers;
    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_blas_scratch_buffers;
};

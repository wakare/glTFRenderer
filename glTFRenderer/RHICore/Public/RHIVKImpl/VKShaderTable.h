#pragma once
#include "RHIInterface/IRHIShaderTable.h"
#include "VolkUtils.h"

#include <memory>

class IRHIBufferAllocation;

class RHICORE_API VKShaderTable : public IRHIShaderTable
{
public:
    virtual bool InitShaderTable(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIPipelineStateObject& pso, IRHIRayTracingAS& as, const std::vector<RHIShaderBindingTable>& sbts) override;

    const VkStridedDeviceAddressRegionKHR& GetRayGenRegion() const { return m_raygen_region; }
    const VkStridedDeviceAddressRegionKHR& GetMissRegion() const { return m_miss_region; }
    const VkStridedDeviceAddressRegionKHR& GetHitGroupRegion() const { return m_hit_group_region; }
    const VkStridedDeviceAddressRegionKHR& GetCallableRegion() const { return m_callable_region; }

protected:
    std::shared_ptr<IRHIBufferAllocation> m_raygen_shader_table;
    std::shared_ptr<IRHIBufferAllocation> m_miss_shader_table;
    std::shared_ptr<IRHIBufferAllocation> m_hit_group_shader_table;

    VkStridedDeviceAddressRegionKHR m_raygen_region{};
    VkStridedDeviceAddressRegionKHR m_miss_region{};
    VkStridedDeviceAddressRegionKHR m_hit_group_region{};
    VkStridedDeviceAddressRegionKHR m_callable_region{};
};

#include "VKRayTracingAS.h"

#include "VKDescriptorManager.h"

void VKRayTracingAS::SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc)
{
    m_scene_desc = scene_desc;
}

bool VKRayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list,
                                      IRHIMemoryManager& memory_manager)
{
    return true;
}

const IRHIDescriptorAllocation& VKRayTracingAS::GetTLASDescriptorSRV() const
{
    static VKBufferDescriptorAllocation _dunmmy;
    return _dunmmy;
}

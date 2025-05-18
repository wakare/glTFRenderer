#include "VKRayTracingAS.h"

#include "VKDescriptorManager.h"

bool VKRayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list,
                                      const glTFRenderMeshManager& mesh_manager, IRHIMemoryManager& memory_manager)
{
    return true;
}

const IRHIDescriptorAllocation& VKRayTracingAS::GetTLASDescriptorSRV() const
{
    static VKBufferDescriptorAllocation _dunmmy;
    return _dunmmy;
}

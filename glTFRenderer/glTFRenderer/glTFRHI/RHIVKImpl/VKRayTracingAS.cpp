#include "VKRayTracingAS.h"

#include "VKDescriptorManager.h"

bool VKRayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list,
                                      const glTFRenderMeshManager& mesh_manager, glTFRenderResourceManager& resource_manager)
{
    return true;
}

const IRHIDescriptorAllocation& VKRayTracingAS::GetTLASHandle() const
{
    static VKDescriptorAllocation _dunmmy;
    return _dunmmy;
}

#include "VKRayTracingAS.h"

bool VKRayTracingAS::InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list,
    const glTFRenderMeshManager& mesh_manager)
{
    return true;
}

unsigned long long VKRayTracingAS::GetTLASHandle() const
{
    return 0;
}

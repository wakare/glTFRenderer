#pragma once
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"

class VKRayTracingAS : public IRHIRayTracingAS
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager, glTFRenderResourceManager
                                  & resource_manager) override;
    virtual const IRHIDescriptorAllocation& GetTLASHandle() const override;
};

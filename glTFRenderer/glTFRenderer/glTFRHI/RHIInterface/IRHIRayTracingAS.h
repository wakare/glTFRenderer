#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIDescriptorAllocation;
class glTFRenderMeshManager;

class IRHIRayTracingAS : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRayTracingAS)
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager, glTFRenderResourceManager
                                  & resource_manager) = 0;
    virtual const IRHIDescriptorAllocation& GetTLASHandle() const = 0;
};

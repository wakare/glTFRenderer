#pragma once
#include "RHIInterface/IRHICommandList.h"
#include "RHIInterface/IRHIResource.h"

class IRHIDescriptorAllocation;

class RHICORE_API IRHIRayTracingAS
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRayTracingAS)
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) = 0;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const = 0;
};

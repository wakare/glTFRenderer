#pragma once
#include "IRHIRayTracingAS.h"

class VKRayTracingAS : public IRHIRayTracingAS
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) override;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const override;
};

#pragma once
#include "RHIInterface/IRHIRayTracingAS.h"

class RHICORE_API VKRayTracingAS : public IRHIRayTracingAS
{
public:
    virtual void SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc) override;
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) override;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const override;

private:
    RHIRayTracingSceneDesc m_scene_desc;
};

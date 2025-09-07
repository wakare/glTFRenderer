#pragma once
#include "RHIInterface/IRHIPipelineStateObject.h"

class RHICORE_API VKRTPipelineStateObject : public IRHIRayTracingPipelineStateObject
{
public:
    VKRTPipelineStateObject() = default;
    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::map<RHIShaderType,
                                         std::shared_ptr<IRHIShader>>& shaders) override;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
};

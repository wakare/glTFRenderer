#include "VKRTPipelineStateObject.h"

bool VKRTPipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
                                                      const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::map<RHIShaderType, std::shared_ptr<
                                                      IRHIShader>>& shaders)
{
    return true;
}

bool VKRTPipelineStateObject::Release(IRHIMemoryManager& memory_manager)
{
    return true;
}

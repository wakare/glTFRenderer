#include "VKRTPipelineStateObject.h"

bool VKRTPipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
                                                      const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain)
{
    return true;
}

bool VKRTPipelineStateObject::Release(glTFRenderResourceManager&)
{
    return true;
}

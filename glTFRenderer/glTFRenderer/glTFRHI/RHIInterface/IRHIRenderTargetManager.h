#pragma once
#include <map>
#include <memory>
#include <vector>

#include "IRHICommandList.h"
#include "IRHIRenderTarget.h"
#include "IRHISwapChain.h"

class IRHIRenderTargetManager : public IRHIResource
{
public:
     DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRenderTargetManager)
     
     virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) = 0;
     
     virtual std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTarget(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const
                                                                  RHITextureDesc& desc, const RHIRenderTargetDesc& render_target_desc) = 0;
     virtual std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> CreateRenderTargetFromSwapChain(IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHISwapChain& swap_chain, RHITextureClearValue clear_value) = 0;
     virtual bool ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets) = 0;
     virtual bool BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets, /*optional*/ IRHIDescriptorAllocation* depth_stencil) = 0;
};

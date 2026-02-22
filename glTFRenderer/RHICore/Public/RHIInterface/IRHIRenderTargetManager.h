#pragma once
#include <memory>
#include <vector>

#include "RHIInterface/IRHISwapChain.h"

class IRHIDescriptorAllocation;

class RHICORE_API IRHIRenderTargetManager
{
public:
     IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRenderTargetManager)
     
     virtual bool InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count) = 0;
     
     virtual std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTarget(IRHIDevice& device, IRHIMemoryManager& memory_manager, const RHITextureDesc& desc, RHIDataFormat format = RHIDataFormat::UNKNOWN) = 0;
     virtual std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHIMemoryManager& memory_manager, IRHISwapChain& swap_chain, RHITextureClearValue clear_value) = 0;
     virtual bool ReleaseSwapchainRenderTargets(IRHIMemoryManager& memory_manager) { return true; }
     virtual bool ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets) = 0;
     virtual bool BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIDescriptorAllocation*>& render_targets) = 0;
};

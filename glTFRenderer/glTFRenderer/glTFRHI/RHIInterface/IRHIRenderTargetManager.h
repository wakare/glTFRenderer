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
     
     virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) = 0;
     
     virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat resourceFormat, RHIDataFormat descriptorFormat, const RHIRenderTargetDesc& desc) = 0;
     virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue) = 0;
     virtual bool ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets) = 0;
     virtual bool BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets, /*optional*/ IRHIRenderTarget* depthStencil) = 0;

     bool RegisterRenderTargetWithTag(const std::string& render_target_tag, std::shared_ptr<IRHIRenderTarget> render_target, unsigned back_buffer_index = 0);
     std::shared_ptr<IRHIRenderTarget> GetRenderTargetWithTag(const std::string& render_target_tag, unsigned back_buffer_index = 0) const;
     
protected:
     std::map<std::string, std::shared_ptr<IRHIRenderTarget>> m_register_render_targets;
};

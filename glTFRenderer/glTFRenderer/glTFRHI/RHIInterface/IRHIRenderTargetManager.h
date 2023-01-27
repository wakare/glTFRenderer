#pragma once
#include <memory>
#include <vector>

#include "IRHICommandList.h"
#include "IRHIRenderTarget.h"
#include "IRHISwapChain.h"

class IRHIRenderTargetManager : public IRHIResource
{
public:
     virtual bool InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount) = 0;
     
     virtual std::shared_ptr<IRHIRenderTarget> CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type, RHIDataFormat format, const IRHIRenderTargetDesc& desc) = 0;
     virtual std::vector<std::shared_ptr<IRHIRenderTarget>> CreateRenderTargetFromSwapChain(IRHIDevice& device, IRHISwapChain& swapChain, IRHIRenderTargetClearValue clearValue) = 0;
     virtual bool ClearRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray, size_t renderTargetArrayCount) = 0;
     virtual bool BindRenderTarget(IRHICommandList& commandList, IRHIRenderTarget* renderTargetArray, size_t renderTargetArraySize, /*optional*/ IRHIRenderTarget* depthStencil ) = 0;
};

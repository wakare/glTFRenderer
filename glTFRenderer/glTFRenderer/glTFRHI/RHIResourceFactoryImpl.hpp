#pragma once

#include "RHIConfigSingleton.h"
#include "RHIResourceFactory.h"
#include "RHIDX12Impl/DX12CommandAllocator.h"
#include "RHIDX12Impl/DX12CommandQueue.h"
#include "RHIDX12Impl/DX12Device.h"
#include "RHIDX12Impl/DX12Factory.h"
#include "RHIDX12Impl/DX12RenderTarget.h"
#include "RHIDX12Impl/DX12RenderTargetManager.h"
#include "RHIDX12Impl/DX12SwapChain.h"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandQueue.h"
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIFactory.h"
#include "RHIInterface/IRHIRenderTarget.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHISwapChain.h"

inline RHIGraphicsAPIType GetGraphicsAPI() {return RHIConfigSingleton::Instance().GetGraphicsAPIType();}

#define IMPLEMENT_CREATE_RHI_RESOURCE(IRHIResourceType, DXResourceType) \
template <> \
std::shared_ptr<IRHIResourceType> RHIResourceFactory::CreateRHIResource() \
{ \
std::shared_ptr<IRHIResourceType> result = nullptr; \
switch (GetGraphicsAPI()) \
{ \
case RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12: \
result = std::make_shared<DXResourceType>(); \
break; \
} \
return result; \
} \

IMPLEMENT_CREATE_RHI_RESOURCE(IRHIFactory, DX12Factory)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDevice, DX12Device)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandQueue, DX12CommandQueue)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHISwapChain, DX12SwapChain)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTarget, DX12RenderTarget)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTargetManager, DX12RenderTargetManager)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandAllocator, DX12CommandAllocator)


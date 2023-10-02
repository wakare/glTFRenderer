#pragma once

#include "RHIConfigSingleton.h"
#include "RHIResourceFactory.h"
#include "RHIUtils.h"
#include "RHIDX12Impl/DX12CommandAllocator.h"
#include "RHIDX12Impl/DX12CommandList.h"
#include "RHIDX12Impl/DX12CommandQueue.h"
#include "RHIDX12Impl/DX12Device.h"
#include "RHIDX12Impl/DX12Factory.h"
#include "RHIDX12Impl/DX12Fence.h"
#include "RHIDX12Impl/DX12GPUBuffer.h"
#include "RHIDX12Impl/DX12GPUBufferManager.h"
#include "RHIDX12Impl/DX12IndexBufferView.h"
#include "RHIDX12Impl/DX12PipelineStateObject.h"
#include "RHIDX12Impl/DX12RenderTarget.h"
#include "RHIDX12Impl/DX12RenderTargetManager.h"
#include "RHIDX12Impl/DX12RootSignature.h"
#include "RHIDX12Impl/DX12SwapChain.h"
#include "RHIDX12Impl/DX12Utils.h"
#include "RHIDX12Impl/DX12VertexBufferView.h"
#include "RHIDX12Impl/DX12DescriptorHeap.h"
#include "RHIDX12Impl/DX12IndexBuffer.h"
#include "RHIDX12Impl/DX12ShaderTable.h"
#include "RHIDX12Impl/DX12Texture.h"
#include "RHIDX12Impl/DX12VertexBuffer.h"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandQueue.h"
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIFactory.h"
#include "RHIInterface/IRHIFence.h"
#include "RHIInterface/IRHIGPUBuffer.h"
#include "RHIInterface/IRHIRenderTarget.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHIRootSignature.h"
#include "RHIInterface/IRHISwapChain.h"
#include "RHIInterface/IRHIPipelineStateObject.h"

inline RHIGraphicsAPIType GetGraphicsAPI() {return RHIConfigSingleton::Instance().GetGraphicsAPIType();}

#define IMPLEMENT_CREATE_RHI_RESOURCE(IRHIResourceType, DXResourceType) \
template <> \
inline std::shared_ptr<IRHIResourceType> RHIResourceFactory::CreateRHIResource() \
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

IMPLEMENT_CREATE_RHI_RESOURCE(RHIUtils, DX12Utils)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIFactory, DX12Factory)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDevice, DX12Device)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandList, DX12CommandList)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandQueue, DX12CommandQueue)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandAllocator, DX12CommandAllocator)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHISwapChain, DX12SwapChain)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTarget, DX12RenderTarget)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTargetManager, DX12RenderTargetManager)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRootParameter, DX12RootParameter)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIStaticSampler, DX12StaticSampler)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRootSignature, DX12RootSignature)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIGraphicsPipelineStateObject, DX12GraphicsPipelineStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIComputePipelineStateObject, DX12ComputePipelineStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRayTracingPipelineStateObject, DX12DXRStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIShaderTable, DX12ShaderTable)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIFence, DX12Fence)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIGPUBuffer, DX12GPUBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIGPUBufferManager, DX12GPUBufferManager)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIShader, DX12Shader)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIVertexBuffer, DX12VertexBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIVertexBufferView, DX12VertexBufferView)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIIndexBuffer, DX12IndexBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIIndexBufferView, DX12IndexBufferView)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDescriptorHeap, DX12DescriptorHeap)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHITexture, DX12Texture)

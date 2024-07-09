#pragma once

#include "RHIConfigSingleton.h"
#include "RHIResourceFactory.h"
#include "RHIUtils.h"

// Interfaces
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandQueue.h"
#include "RHIInterface/IRHIDevice.h"
#include "RHIInterface/IRHIFactory.h"
#include "RHIInterface/IRHIFence.h"
#include "RHIInterface/IRHIBuffer.h"
#include "RHIInterface/IRHIRenderTarget.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHIRootSignature.h"
#include "RHIInterface/IRHISwapChain.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIRayTracingAS.h"
#include "RHIInterface/IRHISemaphore.h"
#include "RHIInterface/IRHIRenderPass.h"
#include "RHIInterface/IRHIMemoryAllocator.h"

// DX12 implements
#include "RHIDX12Impl/DX12Utils.h"
#include "RHIDX12Impl/DX12CommandAllocator.h"
#include "RHIDX12Impl/DX12CommandList.h"
#include "RHIDX12Impl/DX12CommandQueue.h"
#include "RHIDX12Impl/DX12Device.h"
#include "RHIDX12Impl/DX12Factory.h"
#include "RHIDX12Impl/DX12Fence.h"
#include "RHIDX12Impl/DX12Buffer.h"
#include "RHIDX12Impl/DX12MemoryManager.h"
#include "RHIDX12Impl/DX12IndexBufferView.h"
#include "RHIDX12Impl/DX12PipelineStateObject.h"
#include "RHIDX12Impl/DX12RenderTarget.h"
#include "RHIDX12Impl/DX12RenderTargetManager.h"
#include "RHIDX12Impl/DX12RootSignature.h"
#include "RHIDX12Impl/DX12SwapChain.h"
#include "RHIDX12Impl/DX12VertexBufferView.h"
#include "RHIDX12Impl/DX12DescriptorHeap.h"
#include "RHIDX12Impl/DX12IndexBuffer.h"
#include "RHIDX12Impl/DX12RayTracingAS.h"
#include "RHIDX12Impl/DX12Shader.h"
#include "RHIDX12Impl/DX12ShaderTable.h"
#include "RHIDX12Impl/DX12Texture.h"
#include "RHIDX12Impl/DX12VertexBuffer.h"
#include "RHIDX12Impl/DX12CommandSignature.h"
#include "RHIDX12Impl/Dx12RenderPass.h"
#include "RHIDX12Impl/DX12MemoryAllocator.h"

// VK implements
#include "RHIVKImpl/VulkanUtils.h"
#include "RHIVKImpl/VKCommandList.h"
#include "RHIVKImpl/VKCommandSignature.h"
#include "RHIVKImpl/VKComputePipelineStateObject.h"
#include "RHIVKImpl/VKDescriptorHeap.h"
#include "RHIVKImpl/VKDevice.h"
#include "RHIVKImpl/VKFactory.h"
#include "RHIVKImpl/VKFence.h"
#include "RHIVKImpl/VKGPUBuffer.h"
#include "RHIVKImpl/VKBufferManager.h"
#include "RHIVKImpl/VKIndexBuffer.h"
#include "RHIVKImpl/VKIndexBufferView.h"
#include "RHIVKImpl/VKPipelineStateObject.h"
#include "RHIVKImpl/VKRayTracingAS.h"
#include "RHIVKImpl/VKRenderTarget.h"
#include "RHIVKImpl/VKRenderTargetManager.h"
#include "RHIVKImpl/VKRootParameter.h"
#include "RHIVKImpl/VKRootSignature.h"
#include "RHIVKImpl/VKRTPipelineStateObject.h"
#include "RHIVKImpl/VKSemaphore.h"
#include "RHIVKImpl/VKShader.h"
#include "RHIVKImpl/VKShaderTable.h"
#include "RHIVKImpl/VKStaticSampler.h"
#include "RHIVKImpl/VKSwapChain.h"
#include "RHIVKImpl/VKTexture.h"
#include "RHIVKImpl/VKVertexBuffer.h"
#include "RHIVKImpl/VKVertexBufferView.h"
#include "RHIVKImpl/VKRenderPass.h"
#include "RHIVKImpl/VKMemoryAllocator.h"

inline RHIGraphicsAPIType GetGraphicsAPI() {return RHIConfigSingleton::Instance().GetGraphicsAPIType();}

#define IMPLEMENT_CREATE_RHI_RESOURCE(IRHIResourceType, DX12ResourceType, VKResourceType) \
template <> \
inline std::shared_ptr<IRHIResourceType> RHIResourceFactory::CreateRHIResource() \
{ \
std::shared_ptr<IRHIResourceType> result = nullptr; \
switch (GetGraphicsAPI()) \
{ \
case RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12: \
result = std::make_shared<DX12ResourceType>(); \
break; \
case RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan: \
result = std::make_shared<VKResourceType>(); \
break; \
} \
return result; \
} \

IMPLEMENT_CREATE_RHI_RESOURCE(RHIUtils, DX12Utils, VulkanUtils)

IMPLEMENT_CREATE_RHI_RESOURCE(IRHIFactory, DX12Factory, VKFactory)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDevice, DX12Device, VKDevice)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandList, DX12CommandList, VKCommandList)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandQueue, DX12CommandQueue, VKCommandQueue)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandAllocator, DX12CommandAllocator, VKCommandAllocator)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHISwapChain, DX12SwapChain, VKSwapChain)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTarget, DX12RenderTarget, VKRenderTarget)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderTargetManager, DX12RenderTargetManager, VKRenderTargetManager)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRootParameter, DX12RootParameter, VKRootParameter)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIStaticSampler, DX12StaticSampler, VKStaticSampler)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRootSignature, DX12RootSignature, VKRootSignature)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIGraphicsPipelineStateObject, DX12GraphicsPipelineStateObject, VKGraphicsPipelineStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIComputePipelineStateObject, DX12ComputePipelineStateObject, VKComputePipelineStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRayTracingPipelineStateObject, DX12RTPipelineStateObject, VKRTPipelineStateObject)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIShaderTable, DX12ShaderTable, VKShaderTable)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIFence, DX12Fence, VKFence)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIBuffer, DX12Buffer, VKGPUBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIMemoryManager, DX12MemoryManager, VKBufferManager)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIShader, DX12Shader, VKShader)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIVertexBuffer, DX12VertexBuffer, VKVertexBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIVertexBufferView, DX12VertexBufferView, VKVertexBufferView)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIIndexBuffer, DX12IndexBuffer, VKIndexBuffer)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIIndexBufferView, DX12IndexBufferView, VKIndexBufferView)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDescriptorHeap, DX12DescriptorHeap, VKDescriptorHeap)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHITexture, DX12Texture, VKTexture)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRayTracingAS, DX12RayTracingAS, VKRayTracingAS)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHICommandSignature, DX12CommandSignature, VKCommandSignature)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHISemaphore, RHISemaphoreNull, VKSemaphore)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIRenderPass, DX12RenderPass, VKRenderPass)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIMemoryAllocator, DX12MemoryAllocator, VKMemoryAllocator)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDescriptorAllocation, DX12DescriptorAllocation, VKDescriptorAllocation)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHIDescriptorTable, DX12DescriptorTable, VKDescriptorTable)
IMPLEMENT_CREATE_RHI_RESOURCE(IRHITextureAllocation, DX12TextureAllocation, VKTextureAllocation)

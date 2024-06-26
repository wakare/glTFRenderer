#include "DX12RenderTargetManager.h"
#include "DX12CommandList.h"
#include "DX12ConverterUtils.h"
#include "DX12DescriptorHeap.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

UINT GetDescriptorIncSize(ID3D12Device* device, RHIRenderTargetType type)
{
    UINT result = 0;
    switch (type) {
    case RHIRenderTargetType::RTV:
        result = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        break;
    case RHIRenderTargetType::DSV:
        result = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        break;
    case RHIRenderTargetType::Unknown:
        break;
    default: GLTF_CHECK(false);
    }
    return result;
}

DX12RenderTargetManager::DX12RenderTargetManager()
= default;

DX12RenderTargetManager::~DX12RenderTargetManager()
= default;

bool DX12RenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t max_render_target_count)
{
    return true;
}

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTarget(IRHIDevice& device, RHIRenderTargetType type,
                                                                              RHIDataFormat resource_format, RHIDataFormat descriptor_format, const RHITextureDesc& desc)
{
    std::shared_ptr<IRHITextureAllocation> texture_allocation;
    glTFRenderResourceManager::GetMemoryManager().AllocateTextureMemory(device, desc, texture_allocation);

    return CreateRenderTargetWithResource(device, type, descriptor_format, texture_allocation->m_texture,
        DX12ConverterUtils::ConvertToD3DClearValue(desc.GetClearValue()));
}

std::vector<std::shared_ptr<IRHIRenderTarget>> DX12RenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHISwapChain& swapChain, RHITextureClearValue clearValue)
{
    auto* dxSwapChain = dynamic_cast<DX12SwapChain&>(swapChain).GetSwapChain();

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    dxSwapChain->GetDesc(&swapChainDesc);

    std::vector<std::shared_ptr<IRHIRenderTarget>> outVector = {};

    D3D12_CLEAR_VALUE dx_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(clearValue);

    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        ID3D12Resource* resource = nullptr;
        dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource));
        std::shared_ptr<IRHITexture> external_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
        dynamic_cast<DX12Texture&>(*external_texture).InitFromExternalResource(resource,
            {
                "SwapChain_back_buffer",
                swapChain.GetWidth(),
                swapChain.GetHeight(),
                swapChain.GetBackBufferFormat(),
                static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
                {
                    .clear_format = swapChain.GetBackBufferFormat(),
                    .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
                }
            });
        m_external_textures.push_back(external_texture);
        
        auto render_target = CreateRenderTargetWithResource(device, RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, external_texture, dx_clear_value);
        outVector.push_back(render_target);
    }

    return outVector;
}

bool DX12RenderTargetManager::ClearRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    for (size_t i = 0; i < renderTargets.size(); ++i)
    {
        auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(renderTargets[i]);
        auto dx12_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(dxRenderTarget->GetClearValue());
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(dxRenderTarget->GetDescriptorAllocation()).m_cpu_handle;
        
        switch (dxRenderTarget->GetRenderTargetType())
        {
        case RHIRenderTargetType::RTV:
            {
                dxCommandList->ClearRenderTargetView({handle}, dx12_clear_value.Color, 0, nullptr);    
            }
            break;

        case RHIRenderTargetType::DSV:
            {
                dxCommandList->ClearDepthStencilView({handle}, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                    dx12_clear_value.DepthStencil.Depth, dx12_clear_value.DepthStencil.Stencil, 0, nullptr);
            }
            break;
            
        default:
            GLTF_CHECK(false);
        }
    }

    return true;
}

bool DX12RenderTargetManager::BindRenderTarget(IRHICommandList& commandList, const std::vector<IRHIRenderTarget*>& renderTargets,
                                               IRHIRenderTarget* depthStencil)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    if (renderTargets.empty() && !depthStencil)
    {
        // Bind zero rt? some bugs must exists.. 
        assert(false);
        return false;
    }
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetViews(renderTargets.size());
    for (size_t i = 0; i < renderTargetViews.size(); ++i)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(renderTargets[i]->GetDescriptorAllocation()).m_cpu_handle;
        renderTargetViews[i] = {handle};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsHandle{};
    if (depthStencil)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(depthStencil->GetDescriptorAllocation()).m_cpu_handle;
        dsHandle = {handle};
    }

    // TODO: Check RTsSingleHandleToDescriptorRange means?
    dxCommandList->OMSetRenderTargets(renderTargetViews.size(), renderTargetViews.data(), false, depthStencil ? &dsHandle : nullptr);
    
    return true;
}

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTargetWithResource(IRHIDevice& device, RHIRenderTargetType type,
    RHIDataFormat descriptor_format, std::shared_ptr<IRHITexture> texture_resource, const D3D12_CLEAR_VALUE& clear_value)
{
    GLTF_CHECK(texture_resource->GetTextureDesc().GetUsage() & RUF_ALLOW_RENDER_TARGET || texture_resource->GetTextureDesc().GetUsage() & RUF_ALLOW_DEPTH_STENCIL);
    const bool is_rtv = (type == RHIRenderTargetType::RTV);
    auto& descriptor_heap = glTFRenderResourceManager::GetMemoryManager().GetDescriptorHeap(is_rtv ? RHIDescriptorHeapType::RTV : RHIDescriptorHeapType::DSV);
    std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation;
    descriptor_heap.CreateResourceDescriptorInHeap(device, *texture_resource, 
                                                             {
                                                                 .format = descriptor_format,
                                                                 .dimension = RHIResourceDimension::TEXTURE2D,
                                                                 .view_type = is_rtv ? RHIViewType::RVT_RTV : RHIViewType::RVT_DSV 
                                                             },
                                                             descriptor_allocation);
    
    std::shared_ptr<IRHIRenderTarget> render_target = RHIResourceFactory::CreateRHIResource<IRHIRenderTarget>();
    render_target->InitRenderTarget(texture_resource, descriptor_allocation);
    
    return render_target;
}

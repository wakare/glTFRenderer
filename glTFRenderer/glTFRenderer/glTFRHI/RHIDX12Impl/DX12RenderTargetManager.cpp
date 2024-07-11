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

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTarget(IRHIDevice& device, glTFRenderResourceManager& resource_manager, const RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc)
{
    std::shared_ptr<IRHITextureAllocation> texture_allocation;
    resource_manager.GetMemoryManager().AllocateTextureMemory(device, resource_manager, texture_desc, texture_allocation);

    return CreateRenderTargetWithResource(device, resource_manager, render_target_desc.type, render_target_desc.format == RHIDataFormat::UNKNOWN ? texture_desc.GetDataFormat() : render_target_desc.format,
                                          texture_allocation, DX12ConverterUtils::ConvertToD3DClearValue(texture_desc.GetClearValue()));
}

std::vector<std::shared_ptr<IRHIRenderTarget>> DX12RenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHISwapChain& swap_chain, RHITextureClearValue clear_value)
{
    auto* dxSwapChain = dynamic_cast<DX12SwapChain&>(swap_chain).GetSwapChain();

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    dxSwapChain->GetDesc(&swapChainDesc);

    std::vector<std::shared_ptr<IRHIRenderTarget>> outVector = {};

    D3D12_CLEAR_VALUE dx_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(clear_value);

    for (UINT i = 0; i < swapChainDesc.BufferCount; ++i)
    {
        ID3D12Resource* resource = nullptr;
        dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&resource));
        std::shared_ptr<IRHITexture> external_texture = RHIResourceFactory::CreateRHIResource<IRHITexture>();
        dynamic_cast<DX12Texture&>(*external_texture).InitFromExternalResource(resource,
            {
                "SwapChain_back_buffer",
                swap_chain.GetWidth(),
                swap_chain.GetHeight(),
                swap_chain.GetBackBufferFormat(),
                static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
                {
                    .clear_format = swap_chain.GetBackBufferFormat(),
                    .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
                }
            });
        std::shared_ptr<IRHITextureAllocation> external_texture_allocation = RHIResourceFactory::CreateRHIResource<IRHITextureAllocation>();
        external_texture_allocation->m_texture = external_texture;
        m_external_textures.push_back(external_texture_allocation);
        
        auto render_target = CreateRenderTargetWithResource(device, resource_manager, RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM, external_texture_allocation, dx_clear_value);
        outVector.push_back(render_target);
    }

    return outVector;
}

bool DX12RenderTargetManager::ClearRenderTarget(IRHICommandList& command_list, const std::vector<IRHIRenderTarget*>& render_targets)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    for (size_t i = 0; i < render_targets.size(); ++i)
    {
        auto* dxRenderTarget = dynamic_cast<DX12RenderTarget*>(render_targets[i]);
        auto dx12_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(dxRenderTarget->GetClearValue());
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(dxRenderTarget->GetDescriptorAllocation()).m_cpu_handle;

        GLTF_CHECK(render_targets[i]->GetTexture().GetState() == RHIResourceStateType::STATE_RENDER_TARGET ||
            render_targets[i]->GetTexture().GetState() == RHIResourceStateType::STATE_DEPTH_WRITE);
        
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

bool DX12RenderTargetManager::ClearRenderTarget(IRHICommandList& command_list,
                                                const std::vector<IRHIDescriptorAllocation*>& render_targets, const RHITextureClearValue&
                                                render_target_clear_value, const RHITextureClearValue& depth_stencil_clear_value)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    for (size_t i = 0; i < render_targets.size(); ++i)
    {
        auto& render_target = *render_targets[i];

        auto dx_render_target_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(render_target_clear_value);
        auto dx_depth_stencil_clear_value = DX12ConverterUtils::ConvertToD3DClearValue(depth_stencil_clear_value);
        
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(render_target).m_cpu_handle;
        switch (render_target.m_view_desc.view_type)
        {
        case RHIViewType::RVT_RTV:
            {
                dxCommandList->ClearRenderTargetView({handle}, dx_render_target_clear_value.Color, 0, nullptr);    
            }
            break;

        case RHIViewType::RVT_DSV:
            {
                dxCommandList->ClearDepthStencilView({handle}, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                    dx_render_target_clear_value.DepthStencil.Depth, dx_depth_stencil_clear_value.DepthStencil.Stencil, 0, nullptr);
            }
            break;
            
        default:
            GLTF_CHECK(false);
        }
    }

    return true;
}

bool DX12RenderTargetManager::BindRenderTarget(IRHICommandList& command_list, const std::vector<IRHIRenderTarget*>& render_targets,
                                               IRHIRenderTarget* depth_stencil)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    if (render_targets.empty() && !depth_stencil)
    {
        // Bind zero rt? some bugs must exists.. 
        assert(false);
        return false;
    }
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> render_target_views(render_targets.size());
    for (size_t i = 0; i < render_target_views.size(); ++i)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(render_targets[i]->GetDescriptorAllocation()).m_cpu_handle;
        render_target_views[i] = {handle};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsHandle{};
    if (depth_stencil)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(depth_stencil->GetDescriptorAllocation()).m_cpu_handle;
        dsHandle = {handle};
    }

    // TODO: Check RTsSingleHandleToDescriptorRange means?
    dxCommandList->OMSetRenderTargets(render_target_views.size(), render_target_views.data(), false, depth_stencil ? &dsHandle : nullptr);
    
    return true;
}

bool DX12RenderTargetManager::BindRenderTarget(IRHICommandList& command_list,
                                               const std::vector<IRHIDescriptorAllocation*>& render_targets,
                                               IRHIDescriptorAllocation* depth_stencil)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(command_list).GetCommandList();
    if (render_targets.empty() && !depth_stencil)
    {
        // Bind zero rt? some bugs must exists.. 
        assert(false);
        return false;
    }
    
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> render_target_views(render_targets.size());
    for (size_t i = 0; i < render_target_views.size(); ++i)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(*render_targets[i]).m_cpu_handle;
        render_target_views[i] = {handle};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsHandle{};
    if (depth_stencil)
    {
        auto handle = dynamic_cast<DX12DescriptorAllocation&>(*depth_stencil).m_cpu_handle;
        dsHandle = {handle};
    }

    // TODO: Check RTsSingleHandleToDescriptorRange means?
    dxCommandList->OMSetRenderTargets(render_target_views.size(), render_target_views.data(), false, depth_stencil ? &dsHandle : nullptr);
    
    return true;
}

std::shared_ptr<IRHIRenderTarget> DX12RenderTargetManager::CreateRenderTargetWithResource(IRHIDevice& device, glTFRenderResourceManager& resource_manager,
    RHIRenderTargetType type, RHIDataFormat descriptor_format, std::shared_ptr<IRHITextureAllocation> texture_resource, const D3D12_CLEAR_VALUE& clear_value)
{
    GLTF_CHECK(texture_resource->m_texture->GetTextureDesc().GetUsage() & RUF_ALLOW_RENDER_TARGET ||
        texture_resource->m_texture->GetTextureDesc().GetUsage() & RUF_ALLOW_DEPTH_STENCIL);
    
    const bool is_rtv = (type == RHIRenderTargetType::RTV);
    std::shared_ptr<IRHIDescriptorAllocation> descriptor_allocation;
    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(device, *texture_resource->m_texture, 
        {
            .format = descriptor_format,
            .dimension = RHIResourceDimension::TEXTURE2D,
            .view_type = is_rtv ? RHIViewType::RVT_RTV : RHIViewType::RVT_DSV,
        },
        descriptor_allocation);
    
    std::shared_ptr<IRHIRenderTarget> render_target = RHIResourceFactory::CreateRHIResource<IRHIRenderTarget>();
    render_target->InitRenderTarget(texture_resource, descriptor_allocation);
    
    return render_target;
}

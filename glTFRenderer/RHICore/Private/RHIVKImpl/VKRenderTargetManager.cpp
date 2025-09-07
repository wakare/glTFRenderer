#include "VKRenderTargetManager.h"

#include "IRHIMemoryManager.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "VKDevice.h"
#include "VKSwapChain.h"

bool VKRenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount)
{
    return false;
}

std::shared_ptr<IRHITextureDescriptorAllocation> VKRenderTargetManager::CreateRenderTarget(IRHIDevice& device,
    IRHIMemoryManager& memory_manager, const RHITextureDesc& texture_desc, RHIDataFormat format)
{
    std::shared_ptr<IRHITextureAllocation> out_texture_allocation;
    memory_manager.AllocateTextureMemory(device, texture_desc, out_texture_allocation);

    format = format == RHIDataFormat::UNKNOWN ? texture_desc.GetDataFormat() : format;
    
    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor_allocation;
    RHITextureDescriptorDesc render_target(format, RHIResourceDimension::TEXTURE2D,
        IsDepthStencilFormat(format) ? RHIViewType::RVT_DSV : RHIViewType::RVT_RTV);
    memory_manager.GetDescriptorManager().CreateDescriptor(device, out_texture_allocation->m_texture,
        render_target, texture_descriptor_allocation);
    
    return texture_descriptor_allocation;
}

std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> VKRenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHIMemoryManager& memory_manager, IRHISwapChain& swapChain, RHITextureClearValue clearValue)
{
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> results;

    VKSwapChain& vk_swap_chain = dynamic_cast<VKSwapChain&>(swapChain);
    const unsigned back_buffer_count = vk_swap_chain.GetBackBufferCount();

    results.resize(back_buffer_count);
    for (unsigned i = 0; i < back_buffer_count; ++i)
    {
        auto swap_chain_texture = vk_swap_chain.GetSwapChainImageByIndex(i);
        RHITextureDescriptorDesc render_target(vk_swap_chain.GetBackBufferFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_RTV);
        memory_manager.GetDescriptorManager().CreateDescriptor(device, swap_chain_texture,
                render_target, results[i]);
    }

    return results;
}

bool VKRenderTargetManager::ClearRenderTarget(IRHICommandList& commandList,
                                              const std::vector<IRHIDescriptorAllocation*>& render_targets)
{
    return false;
}

bool VKRenderTargetManager::BindRenderTarget(IRHICommandList& commandList,
                                             const std::vector<IRHIDescriptorAllocation*>& render_targets)
{
    return false;
}

#include "VKRenderTargetManager.h"

#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKRenderTarget.h"
#include "VKSwapChain.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"

bool VKRenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount)
{
    return false;
}

std::shared_ptr<IRHITextureDescriptorAllocation> VKRenderTargetManager::CreateRenderTarget(IRHIDevice& device,
                                                                            glTFRenderResourceManager& resource_manager, const RHITextureDesc& texture_desc, const RHIRenderTargetDesc& render_target_desc)
{
    std::shared_ptr<IRHITextureAllocation> out_texture_allocation;
    resource_manager.GetMemoryManager().AllocateTextureMemory(device, resource_manager, texture_desc, out_texture_allocation);

    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor_allocation;
    RHITextureDescriptorDesc render_target(render_target_desc.format, RHIResourceDimension::TEXTURE2D,
        render_target_desc.type == RHIRenderTargetType::DSV ? RHIViewType::RVT_DSV : RHIViewType::RVT_RTV);
    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(device, out_texture_allocation->m_texture,
        render_target, texture_descriptor_allocation);
    
    return texture_descriptor_allocation;
}

std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> VKRenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, glTFRenderResourceManager& resource_manager, IRHISwapChain& swapChain, RHITextureClearValue clearValue)
{
    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> results;

    VKSwapChain& vk_swap_chain = dynamic_cast<VKSwapChain&>(swapChain);
    const unsigned back_buffer_count = vk_swap_chain.GetBackBufferCount();

    results.resize(back_buffer_count);
    for (unsigned i = 0; i < back_buffer_count; ++i)
    {
        auto swap_chain_texture = vk_swap_chain.GetSwapChainImageByIndex(i);
        RHITextureDescriptorDesc render_target(vk_swap_chain.GetBackBufferFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_RTV);
        resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(device, swap_chain_texture,
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

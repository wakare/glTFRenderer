#include "VKRenderTargetManager.h"

#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKRenderTarget.h"
#include "VKSwapChain.h"

bool VKRenderTargetManager::InitRenderTargetManager(IRHIDevice& device, size_t maxRenderTargetCount)
{
    return true;
}

std::shared_ptr<IRHIRenderTarget> VKRenderTargetManager::CreateRenderTarget(IRHIDevice& device,
    RHIRenderTargetType type, RHIDataFormat resourceFormat, RHIDataFormat descriptorFormat,
    const RHIRenderTargetDesc& desc)
{
    GLTF_CHECK(false);
    return nullptr;
}

std::vector<std::shared_ptr<IRHIRenderTarget>> VKRenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHISwapChain& swapChain, RHIRenderTargetClearValue clearValue)
{
    std::vector<std::shared_ptr<IRHIRenderTarget>> results;

    const VkDevice vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    VKSwapChain& vk_swap_chain = dynamic_cast<VKSwapChain&>(swapChain);
    const unsigned back_buffer_count = vk_swap_chain.GetBackBufferCount();

    results.resize(back_buffer_count);
    for (unsigned i = 0; i < back_buffer_count; ++i)
    {
        const VkImage back_buffer_image = vk_swap_chain.GetSwapChainImageByIndex(i);

        const auto vk_render_target = std::make_shared<VKRenderTarget>();
        vk_render_target->InitRenderTarget(vk_device, VKConverterUtils::ConvertToFormat(vk_swap_chain.GetBackBufferFormat()), back_buffer_image);
        
        results[i] = vk_render_target; 
    }

    return results;
}

bool VKRenderTargetManager::ClearRenderTarget(IRHICommandList& commandList,
    const std::vector<IRHIRenderTarget*>& renderTargets)
{
    return true;
}

bool VKRenderTargetManager::BindRenderTarget(IRHICommandList& commandList,
    const std::vector<IRHIRenderTarget*>& renderTargets, IRHIRenderTarget* depthStencil)
{
    return true;
}

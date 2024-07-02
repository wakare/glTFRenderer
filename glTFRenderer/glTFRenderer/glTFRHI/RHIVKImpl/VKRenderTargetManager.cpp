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
                                                                            const RHITextureDesc& desc)
{
    const VkDevice vk_device = dynamic_cast<VKDevice&>(device).GetDevice();
    const auto vk_render_target = std::make_shared<VKRenderTarget>();

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D; 
    image_create_info.extent = {desc.GetTextureWidth(), desc.GetTextureHeight(), 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_create_info.format = VKConverterUtils::ConvertToFormat(resourceFormat);
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 

    VkImage image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(vk_device, &image_create_info, nullptr, &image);
    GLTF_CHECK(result == VK_SUCCESS);
    
    vk_render_target->InitRenderTarget(vk_device, VKConverterUtils::ConvertToFormat(resourceFormat), image);
    return nullptr;
}

std::vector<std::shared_ptr<IRHIRenderTarget>> VKRenderTargetManager::CreateRenderTargetFromSwapChain(
    IRHIDevice& device, IRHISwapChain& swapChain, RHITextureClearValue clearValue)
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

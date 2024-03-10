#include "VulkanUtils.h"

#include "VKCommandList.h"
#include "VKConverterUtils.h"
#include "VKFence.h"
#include "VKFrameBuffer.h"
#include "VKRenderPass.h"
#include "VKSemaphore.h"

bool VulkanUtils::InitGUIContext(IRHIDevice& device, IRHIDescriptorHeap& descriptor_heap, unsigned back_buffer_count)
{
    return true;
}

bool VulkanUtils::BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info)
{
    const auto vk_command_buffer = dynamic_cast<const VKCommandList&>(command_list).GetCommandBuffer();
    const auto vk_render_pass = dynamic_cast<const VKRenderPass&>(*begin_render_pass_info.render_pass).GetRenderPass();
    const auto vk_frame_buffer = dynamic_cast<const VKFrameBuffer&>(*begin_render_pass_info.frame_buffer).GetFrameBuffer();
        
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = vk_render_pass;
    render_pass_begin_info.framebuffer = vk_frame_buffer;
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = {begin_render_pass_info.swap_chain->GetWidth(), begin_render_pass_info.swap_chain->GetHeight()};
    const VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    vkCmdBeginRenderPass(vk_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    return true;
}

bool VulkanUtils::EndRenderPass(IRHICommandList& command_list)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    vkCmdEndRenderPass(vk_command_buffer);

    return true;
}

bool VulkanUtils::ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator,
                                   IRHIPipelineStateObject* initPSO)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(commandList).GetCommandBuffer();
    vkResetCommandBuffer(vk_command_buffer, 0);

    return commandList.BeginRecordCommandList();
}

bool VulkanUtils::CloseCommandList(IRHICommandList& commandList)
{
    return commandList.EndRecordCommandList();
}

bool VulkanUtils::ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    const auto vk_fence = dynamic_cast<VKFence&>(dynamic_cast<VKCommandList&>(command_list).GetFence()).GetFence();
    const auto vk_command_queue= dynamic_cast<VKCommandQueue&>(command_queue).GetGraphicsQueue(); 
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> wait_semaphores(context.wait_infos.size());
    std::vector<VkPipelineStageFlags> wait_stages(context.wait_infos.size());

    for (size_t i = 0; i < context.wait_infos.size(); ++i)
    {
        const auto& wait_info = context.wait_infos[i];
        wait_semaphores[i] = dynamic_cast<const VKSemaphore&>(*wait_info.m_wait_semaphore).GetSemaphore();
        wait_stages[i] = VKConverterUtils::ConvertToPipelineStage(wait_info.wait_stage);
    }
    
    submit_info.waitSemaphoreCount = context.wait_infos.size();
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_command_buffer;

    std::vector<VkSemaphore> sign_semaphores(context.sign_semaphores.size());
    for (size_t i = 0; i < context.sign_semaphores.size(); ++i)
    {
        sign_semaphores[i] = dynamic_cast<const VKSemaphore&>(*context.sign_semaphores[i]).GetSemaphore();
    }
    
    submit_info.signalSemaphoreCount = context.sign_semaphores.size();
    submit_info.pSignalSemaphores = sign_semaphores.data();
    
    const VkResult result = vkQueueSubmit(vk_command_queue, 1, &submit_info, vk_fence);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

bool VulkanUtils::ResetCommandAllocator(IRHICommandAllocator& commandAllocator)
{
    return true;
}

bool VulkanUtils::WaitCommandListFinish(IRHICommandList& command_list)
{
    return command_list.WaitCommandList();
}

bool VulkanUtils::Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    return swap_chain.Present(command_queue, command_list);
}

bool VulkanUtils::SupportRayTracing(IRHIDevice& device)
{
    return false;
}

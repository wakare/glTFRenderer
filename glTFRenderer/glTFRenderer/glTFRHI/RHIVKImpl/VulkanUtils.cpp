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

bool VulkanUtils::NewGUIFrame()
{
    return true;
}

bool VulkanUtils::RenderGUIFrame(IRHICommandList& commandList)
{
    return true;
}

bool VulkanUtils::ExitGUI()
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
    render_pass_begin_info.renderArea.extent = {begin_render_pass_info.width, begin_render_pass_info.height};
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

bool VulkanUtils::SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature,
    bool isGraphicsPipeline)
{
    return true;
}

bool VulkanUtils::SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewport_desc)
{
    return true;
}

bool VulkanUtils::SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissor_rect)
{
    return true;
}

bool VulkanUtils::SetVertexBufferView(IRHICommandList& commandList, unsigned slot, IRHIVertexBufferView& view)
{
    return true;
}

bool VulkanUtils::SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view)
{
    return true;
}

bool VulkanUtils::SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type)
{
    return true;
}

bool VulkanUtils::SetDescriptorHeapArray(IRHICommandList& commandList, IRHIDescriptorHeap* descriptor_heap_array_data,
    size_t descriptor_heap_array_count)
{
    return true;
}

bool VulkanUtils::SetConstant32BitToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, unsigned* data,
    unsigned count, bool isGraphicsPipeline)
{
    return true;
}

bool VulkanUtils::SetCBVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                            const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    return true;
}

bool VulkanUtils::SetSRVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                            const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    return true;
}

bool VulkanUtils::SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
                                           const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline)
{
    return true;
}

bool VulkanUtils::SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex,
    const IRHIDescriptorTable& table_handle, bool isGraphicsPipeline)
{
    return false;
}

bool VulkanUtils::UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer,
                                                     IRHIBuffer& defaultBuffer, void* data, size_t size)
{
    return true;
}

bool VulkanUtils::UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer,
    IRHIBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch)
{
    return true;
}

bool VulkanUtils::AddBufferBarrierToCommandList(IRHICommandList& commandList, const IRHIBuffer& buffer,
                                                RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    return true;
}

bool VulkanUtils::AddTextureBarrierToCommandList(IRHICommandList& commandList, const IRHITexture& buffer,
                                                 RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    return false;
}

bool VulkanUtils::AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, const IRHIRenderTarget& buffer,
                                                      RHIResourceStateType before_state, RHIResourceStateType after_state)
{
    return true;
}

bool VulkanUtils::DrawInstanced(IRHICommandList& commandList, unsigned vertexCountPerInstance, unsigned instanceCount,
    unsigned startVertexLocation, unsigned startInstanceLocation)
{
    return true;
}

bool VulkanUtils::DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance,
    unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation)
{
    return true;
}

bool VulkanUtils::Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z)
{
    return true;
}

bool VulkanUtils::TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y,
    unsigned Z)
{
    return true;
}

bool VulkanUtils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
    unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset)
{
    return true;
}

bool VulkanUtils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
    unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer,
    unsigned count_buffer_offset)
{
    return true;
}

bool VulkanUtils::Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    return swap_chain.Present(command_queue, command_list);
}

bool VulkanUtils::DiscardResource(IRHICommandList& commandList, IRHIRenderTarget& render_target)
{
    return true;
}

bool VulkanUtils::CopyTexture(IRHICommandList& commandList, IRHITexture& dst, IRHITexture& src)
{
    return true;
}

bool VulkanUtils::CopyBuffer(IRHICommandList& commandList, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src,
    size_t src_offset, size_t size)
{
    return true;
}

bool VulkanUtils::SupportRayTracing(IRHIDevice& device)
{
    return false;
}

unsigned VulkanUtils::GetAlignmentSizeForUAVCount(unsigned size)
{
    return 0;
}

#include "VulkanUtils.h"

#include <backends/imgui_impl_vulkan.h>

#include "VKBuffer.h"
#include "VKCommandList.h"
#include "VKConverterUtils.h"
#include "VKDescriptorManager.h"
#include "VKFence.h"
#include "VKFrameBuffer.h"
#include "VKIndexBufferView.h"
#include "VKPipelineStateObject.h"
#include "VKRenderPass.h"
#include "VKRootSignature.h"
#include "VKSemaphore.h"
#include "VKTexture.h"
#include "VKVertexBufferView.h"

bool VulkanUtils::InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_manager, unsigned back_buffer_count)
{
    auto& vulkan_device = dynamic_cast<VKDevice&>(device);
    auto& vulkan_queue = dynamic_cast<VKCommandQueue&>(graphics_queue);
    auto& vulkan_descriptor_manager = dynamic_cast<VKDescriptorManager&>(descriptor_manager);
    
    ImGui_ImplVulkan_InitInfo vulkan_init_info{};
    vulkan_init_info.Instance = vulkan_device.GetInstance();
    vulkan_init_info.PhysicalDevice = vulkan_device.GetPhysicalDevice(); 
    vulkan_init_info.Device = vulkan_device.GetDevice();
    vulkan_init_info.QueueFamily = vulkan_device.GetGraphicsQueueIndex();
    vulkan_init_info.Queue = vulkan_queue.GetGraphicsQueue();
    vulkan_init_info.PipelineCache = VK_NULL_HANDLE;
    vulkan_init_info.DescriptorPool = vulkan_descriptor_manager.GetDesciptorPool();
    vulkan_init_info.Subpass = 0;
    vulkan_init_info.MinImageCount = back_buffer_count;
    vulkan_init_info.ImageCount = back_buffer_count;
    vulkan_init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    vulkan_init_info.UseDynamicRendering = true;

    VkFormat attachments[] = {VK_FORMAT_R8G8B8A8_UNORM};
    vulkan_init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    vulkan_init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = attachments;
    vulkan_init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    
    ImGui_ImplVulkan_Init(&vulkan_init_info);
    
    return true;
}

bool VulkanUtils::NewGUIFrame()
{
    ImGui_ImplVulkan_NewFrame();
    
    return true;
}

bool VulkanUtils::RenderGUIFrame(IRHICommandList& command_list)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_command_buffer);
    return true;
}

bool VulkanUtils::ExitGUI()
{
    ImGui_ImplVulkan_Shutdown();
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

bool VulkanUtils::BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();

    std::vector<VkRenderingAttachmentInfo> attachment_infos;
    for (auto& render_target : begin_rendering_info.m_render_targets)
    {
        auto& texture_desc = dynamic_cast<const VKTextureDescriptorAllocation&>(*render_target);
        bool render_target_view = texture_desc.GetDesc().m_view_type == RHIViewType::RVT_RTV;
        
        VkRenderingAttachmentInfo attachment { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = texture_desc.GetRawImageView();
        attachment.imageLayout = render_target_view ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
        (begin_rendering_info.enable_depth_write ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
        attachment.clearValue.color = {{1.0, 1.0 ,1.0, 1.0}};

        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        if ((render_target_view && begin_rendering_info.clear_render_target) ||
            (!render_target_view && begin_rendering_info.clear_depth))
        {
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        attachment_infos.push_back(attachment);
    }

    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.colorAttachmentCount = attachment_infos.size();
    rendering_info.pColorAttachments = attachment_infos.data();
    rendering_info.renderArea = {{0, 0}, {begin_rendering_info.rendering_area_width, begin_rendering_info.rendering_area_height}};
    rendering_info.layerCount = 1;
        
    vkCmdBeginRendering(vk_command_buffer, &rendering_info);
    
    return true;
}

bool VulkanUtils::EndRendering(IRHICommandList& command_list)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    vkCmdEndRendering(vk_command_buffer);
    return true;
}

bool VulkanUtils::ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator,
                                   IRHIPipelineStateObject* init_pso)
{
    WaitCommandListFinish(command_list);
    
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    vkResetCommandBuffer(vk_command_buffer, 0);
    
    command_list.BeginRecordCommandList();
    if (init_pso)
    {
        VkPipeline pso {VK_NULL_HANDLE};
        switch(init_pso->GetPSOType())
        {
        case RHIPipelineType::Graphics:
            pso = dynamic_cast<const VKGraphicsPipelineStateObject&>(*init_pso).GetPipeline(); 
            vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
            break;
        case RHIPipelineType::Compute:
            break;
        case RHIPipelineType::RayTracing:
            break;
        case RHIPipelineType::Unknown:
            break;
        }
    }

    
    return true;
}

bool VulkanUtils::CloseCommandList(IRHICommandList& command_list)
{
    return command_list.EndRecordCommandList();
}

bool VulkanUtils::ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto& fence = dynamic_cast<VKFence&>(dynamic_cast<VKCommandList&>(command_list).GetFence());
    const auto vk_fence = fence.GetFence();
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

    fence.SetCanWait(true);
    
    return true;
}

bool VulkanUtils::ResetCommandAllocator(IRHICommandAllocator& command_allocator)
{
    return true;
}

bool VulkanUtils::WaitCommandListFinish(IRHICommandList& command_list)
{
    return command_list.WaitCommandList();
}

bool VulkanUtils::SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature, IRHIPipelineStateObject& pipeline_state_object,
    RHIPipelineType pipeline_type)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto vk_descriptor_set = dynamic_cast<const VKRootSignature&>(root_signature).GetDescriptorSet();
    
    if (pipeline_type == RHIPipelineType::Graphics)
    {
        auto vk_pipeline_layout = dynamic_cast<const VKGraphicsPipelineStateObject&>(pipeline_state_object).GetPipelineLayout();
        vkCmdBindDescriptorSets(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_set, 0, nullptr);
    }
    
    return true;
}

bool VulkanUtils::SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(viewport_desc.width);
    viewport.height = static_cast<float>(viewport_desc.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vk_command_buffer, 0, 1, &viewport);
    
    return true;
}

bool VulkanUtils::SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();

    VkRect2D scissor{};
    scissor.offset = {static_cast<int>(scissor_rect.left), static_cast<int>(scissor_rect.top)};
    scissor.extent = {scissor_rect.right - scissor_rect.left, scissor_rect.bottom - scissor_rect.top };
    vkCmdSetScissor(vk_command_buffer, 0, 1, &scissor);
    
    return true;
}

bool VulkanUtils::SetVertexBufferView(IRHICommandList& command_list, unsigned binding_slot_index, IRHIVertexBufferView& view)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto& vk_vertex_buffer_view = dynamic_cast<const VKVertexBufferView&>(view);

    VkDeviceSize offset = vk_vertex_buffer_view.m_buffer_offset;
    vkCmdBindVertexBuffers(vk_command_buffer, binding_slot_index, 1, &vk_vertex_buffer_view.m_buffer, &offset);
    
    return true;
}

bool VulkanUtils::SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto& vk_index_buffer_view = dynamic_cast<const VKIndexBufferView&>(view);

    VkDeviceSize offset = vk_index_buffer_view.m_desc.offset;
    VkIndexType type = vk_index_buffer_view.m_desc.format == RHIDataFormat::R32_UINT ? VkIndexType::VK_INDEX_TYPE_UINT32 : VkIndexType::VK_INDEX_TYPE_UINT16;
    
    vkCmdBindIndexBuffer(vk_command_buffer, vk_index_buffer_view.m_buffer, offset, type);
    
    return true;
}

bool VulkanUtils::SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type)
{
    return true;
}

bool VulkanUtils::SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, unsigned* data,
    unsigned count, RHIPipelineType pipeline)
{
    return true;
}

bool VulkanUtils::AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer,
                                                RHIResourceStateType before_state, RHIResourceStateType after_state)
{
    
    
    return true;
}

bool VulkanUtils::AddTextureBarrierToCommandList(IRHICommandList& command_list, const IRHITexture& texture,
                                                 RHIResourceStateType before_state, RHIResourceStateType after_state)
{
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto vk_image = dynamic_cast<const VKTexture&>(texture).GetRawImage();
    
    VkImageMemoryBarrier2 image_barrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    image_barrier.pNext = nullptr;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    //image_barrier.oldLayout = VKConverterUtils::ConvertToImageLayout(beforeState);
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VKConverterUtils::ConvertToImageLayout(after_state);

    VkImageAspectFlags aspect_flags = (image_barrier.newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
        VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageSubresourceRange sub_image{};
    sub_image.aspectMask = aspect_flags;
    sub_image.baseMipLevel = 0;
    sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
    sub_image.baseArrayLayer = 0;
    sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;
    
    image_barrier.subresourceRange = sub_image;
    image_barrier.image = vk_image;
    
    VkDependencyInfo dep_info {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext = nullptr;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &image_barrier;
    vkCmdPipelineBarrier2(vk_command_list, &dep_info);
    
    return true;
}

bool VulkanUtils::DrawInstanced(IRHICommandList& command_list, unsigned vertex_count_per_instance, unsigned instance_count,
    unsigned start_vertex_location, unsigned start_instance_location)
{
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    vkCmdDraw(vk_command_list, vertex_count_per_instance, instance_count, start_vertex_location, start_instance_location);
    return true;
}

bool VulkanUtils::DrawIndexInstanced(IRHICommandList& command_list, unsigned index_count_per_instance,
    unsigned instance_count, unsigned start_index_location, unsigned base_vertex_location, unsigned start_instance_location)
{
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    vkCmdDrawIndexed(vk_command_list, index_count_per_instance, instance_count, start_index_location, base_vertex_location, start_instance_location);
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

bool VulkanUtils::CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src)
{
    return true;
}

bool VulkanUtils::CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src,
    size_t src_offset, size_t size)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetCommandBuffer();
    auto src_buffer = dynamic_cast<VKBuffer&>(src).GetRawBuffer();
    auto dst_buffer = dynamic_cast<VKBuffer&>(dst).GetRawBuffer();
    
    VkBufferCopy copy
    {
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = size,
    };
    
    vkCmdCopyBuffer(vk_command_buffer, src_buffer, dst_buffer, 1, &copy);
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

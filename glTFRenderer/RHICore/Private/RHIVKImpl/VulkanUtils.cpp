#include "VulkanUtils.h"

#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#define IMGUI_IMPL_VULKAN_USE_VOLK
#define VK_NO_PROTOTYPES
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
#include "VKCommandQueue.h"
#include "VKComputePipelineStateObject.h"
#include "VolkUtils.h"

VkAccessFlags2 GetAccessFlagFromResourceState(RHIResourceStateType state)
{
    VkAccessFlags2 result{};

    switch (state) {
    case RHIResourceStateType::STATE_UNDEFINED:
        result = VK_ACCESS_2_NONE;
        break;
    case RHIResourceStateType::STATE_COMMON:
        result = VK_ACCESS_2_NONE;
        break;
    case RHIResourceStateType::STATE_GENERIC_READ:
        result = VK_ACCESS_2_MEMORY_READ_BIT;
        break;
    case RHIResourceStateType::STATE_COPY_SOURCE:
        result = VK_ACCESS_2_TRANSFER_READ_BIT;
        break;
    case RHIResourceStateType::STATE_COPY_DEST:
        result = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        break;
    case RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER:
        result = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        break;
    case RHIResourceStateType::STATE_INDEX_BUFFER:
        result = VK_ACCESS_2_INDEX_READ_BIT;
        break;
    case RHIResourceStateType::STATE_PRESENT:
        result = VK_ACCESS_2_MEMORY_READ_BIT;
        break;
    case RHIResourceStateType::STATE_RENDER_TARGET:
        result = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case RHIResourceStateType::STATE_DEPTH_WRITE:
        result = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case RHIResourceStateType::STATE_DEPTH_READ:
        result = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        break;
    case RHIResourceStateType::STATE_UNORDERED_ACCESS:
        result = VK_ACCESS_2_SHADER_WRITE_BIT;
        break;
    case RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE:
        result = VK_ACCESS_2_SHADER_READ_BIT;
        break;
    case RHIResourceStateType::STATE_PIXEL_SHADER_RESOURCE:
        result = VK_ACCESS_2_SHADER_READ_BIT;
        break;
    case RHIResourceStateType::STATE_ALL_SHADER_RESOURCE:
        result = VK_ACCESS_2_SHADER_READ_BIT;
        break;
    case RHIResourceStateType::STATE_RAYTRACING_ACCELERATION_STRUCTURE:
        result = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        break;
    }
    
    return result;
}

bool VulkanUtils::InitGraphicsAPI()
{
    return VolkUtils::InitVolk();
}

bool VulkanUtils::InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_manager, unsigned back_buffer_count)
{
    auto& vulkan_device = dynamic_cast<VKDevice&>(device);
    auto& vulkan_queue = dynamic_cast<VKCommandQueue&>(graphics_queue);
    auto& vulkan_descriptor_manager = dynamic_cast<VKDescriptorManager&>(descriptor_manager);
    VkInstance instance = vulkan_device.GetInstance();
    ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* instance)
    {
        return vkGetInstanceProcAddr(*static_cast<VkInstance*>(instance), function_name);
    }, &instance);
    
    ImGui_ImplVulkan_InitInfo vulkan_init_info{};
    vulkan_init_info.Instance = vulkan_device.GetInstance();
    vulkan_init_info.PhysicalDevice = vulkan_device.GetPhysicalDevice(); 
    vulkan_init_info.Device = vulkan_device.GetDevice();
    vulkan_init_info.QueueFamily = vulkan_device.GetGraphicsQueueIndex();
    vulkan_init_info.Queue = vulkan_queue.GetGraphicsQueue();
    vulkan_init_info.PipelineCache = VK_NULL_HANDLE;
    vulkan_init_info.DescriptorPool = vulkan_descriptor_manager.GetDescriptorPool();
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
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
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
    const auto vk_command_buffer = dynamic_cast<const VKCommandList&>(command_list).GetRawCommandBuffer();
    const auto vk_render_pass = dynamic_cast<const VKRenderPass&>(*begin_render_pass_info.render_pass).GetRenderPass();
    const auto vk_frame_buffer = dynamic_cast<const VKFrameBuffer&>(*begin_render_pass_info.frame_buffer).GetFrameBuffer();
        
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = vk_render_pass;
    render_pass_begin_info.framebuffer = vk_frame_buffer;
    // TODO: Use viewport now
    render_pass_begin_info.renderArea.offset = {begin_render_pass_info.offset_x, begin_render_pass_info.offset_y};
    render_pass_begin_info.renderArea.extent = {begin_render_pass_info.width, begin_render_pass_info.height};
    const VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    vkCmdBeginRenderPass(vk_command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    return true;
}

bool VulkanUtils::EndRenderPass(IRHICommandList& command_list)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    vkCmdEndRenderPass(vk_command_buffer);

    return true;
}

bool VulkanUtils::BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();

    std::vector<VkRenderingAttachmentInfo> color_attachment_infos;
    std::vector<VkRenderingAttachmentInfo> depth_attachment_infos;
    for (auto& render_target : begin_rendering_info.m_render_targets)
    {
        auto& vk_texture_allocation = dynamic_cast<const VKTextureDescriptorAllocation&>(*render_target);
        bool render_target_view = vk_texture_allocation.GetDesc().m_view_type == RHIViewType::RVT_RTV;
        
        VkRenderingAttachmentInfo attachment { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = vk_texture_allocation.GetRawImageView();
        attachment.imageLayout = render_target_view ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
        (begin_rendering_info.enable_depth_write ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);

        const auto& clear_value = vk_texture_allocation.m_source->GetTextureDesc().GetClearValue();
        if (render_target_view)
        {
            attachment.clearValue.color = { {clear_value.clear_color[0], clear_value.clear_color[1], clear_value.clear_color[2], clear_value.clear_color[3]}
            };    
        }
        else
        {
            attachment.clearValue.depthStencil =
                {
                    .depth = clear_value.clear_depth_stencil.clear_depth,
                    .stencil = clear_value.clear_depth_stencil.clear_stencil_value
                };
        }

        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if ((render_target_view && begin_rendering_info.clear_render_target) ||
            (!render_target_view && begin_rendering_info.clear_depth_stencil))
        {
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if (render_target_view)
        {
            color_attachment_infos.push_back(attachment);
        }
        else
        {
            depth_attachment_infos.push_back(attachment);
        }
    }

    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.colorAttachmentCount = color_attachment_infos.size();
    rendering_info.pColorAttachments = color_attachment_infos.data();
    rendering_info.pDepthAttachment = depth_attachment_infos.empty() ? nullptr : depth_attachment_infos.data();
    rendering_info.renderArea = {{begin_rendering_info.rendering_area_offset_x, begin_rendering_info.rendering_area_offset_y}, {begin_rendering_info.rendering_area_width, begin_rendering_info.rendering_area_height}};
    rendering_info.layerCount = 1;
        
    vkCmdBeginRendering(vk_command_buffer, &rendering_info);
    
    return true;
}

bool VulkanUtils::EndRendering(IRHICommandList& command_list)
{
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    vkCmdEndRendering(vk_command_buffer);
    return true;
}

bool VulkanUtils::ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator,
                                   IRHIPipelineStateObject* init_pso)
{
    WaitCommandListFinish(command_list);
    
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
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
            pso = dynamic_cast<const VKComputePipelineStateObject&>(*init_pso).GetPipeline(); 
            vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pso);
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
    const auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
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

bool VulkanUtils::WaitCommandQueueIdle(IRHICommandQueue& command_queue)
{
    VKCommandQueue& vk_command_queue = dynamic_cast<VKCommandQueue&>(command_queue);
    vkQueueWaitIdle(vk_command_queue.GetGraphicsQueue());
    
    return true;
}

bool VulkanUtils::WaitDeviceIdle(IRHIDevice& device)
{
    VKDevice& vk_device = dynamic_cast<VKDevice&>(device);
    VK_CHECK(vkDeviceWaitIdle(vk_device.GetDevice()));
    
    return true;
}

bool VulkanUtils::SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature, IRHIPipelineStateObject& pipeline_state_object,
                                   RHIPipelineType pipeline_type)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto& vk_root_signature = dynamic_cast<const VKRootSignature&>(root_signature);
    const std::vector<VkDescriptorSet>& descriptor_sets = vk_root_signature.GetDescriptorSets();

    if (pipeline_type == RHIPipelineType::Graphics)
    {
        auto vk_pipeline_layout = dynamic_cast<const VKGraphicsPipelineStateObject&>(pipeline_state_object).GetPipelineLayout();
        vkCmdBindDescriptorSets(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
    }

    if (pipeline_type == RHIPipelineType::Compute)
    {
        auto vk_pipeline_layout = dynamic_cast<const VKComputePipelineStateObject&>(pipeline_state_object).GetPipelineLayout();
        vkCmdBindDescriptorSets(vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline_layout, 0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
    }
    
    return true;
}

bool VulkanUtils::SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(viewport_desc.height);
    viewport.width = static_cast<float>(viewport_desc.width);
    viewport.height = -static_cast<float>(viewport_desc.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vk_command_buffer, 0, 1, &viewport);
    
    return true;
}

bool VulkanUtils::SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();

    VkRect2D scissor{};
    scissor.offset = {static_cast<int>(scissor_rect.left), static_cast<int>(scissor_rect.top)};
    scissor.extent = {scissor_rect.right - scissor_rect.left, scissor_rect.bottom - scissor_rect.top };
    vkCmdSetScissor(vk_command_buffer, 0, 1, &scissor);
    
    return true;
}

bool VulkanUtils::SetVertexBufferView(IRHICommandList& command_list, unsigned binding_slot_index, IRHIVertexBufferView& view)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto& vk_vertex_buffer_view = dynamic_cast<const VKVertexBufferView&>(view);

    VkDeviceSize offset = vk_vertex_buffer_view.m_buffer_offset;
    vkCmdBindVertexBuffers(vk_command_buffer, binding_slot_index, 1, &vk_vertex_buffer_view.m_buffer, &offset);
    
    return true;
}

bool VulkanUtils::SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto& vk_index_buffer_view = dynamic_cast<const VKIndexBufferView&>(view);

    VkDeviceSize offset = vk_index_buffer_view.m_desc.offset;
    VkIndexType type = vk_index_buffer_view.m_desc.format == RHIDataFormat::R32_UINT ? VkIndexType::VK_INDEX_TYPE_UINT32 : VkIndexType::VK_INDEX_TYPE_UINT16;
    
    vkCmdBindIndexBuffer(vk_command_buffer, vk_index_buffer_view.m_buffer, offset, type);
    
    return true;
}

bool VulkanUtils::SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type)
{
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    VkPrimitiveTopology topology = VKConverterUtils::ConvertToPrimitiveTopology(type);
    vkCmdSetPrimitiveTopology(vk_command_buffer, topology);
    
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
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto vk_buffer = dynamic_cast<const VKBuffer&>(buffer).GetRawBuffer();
    
    VkBufferMemoryBarrier2 buffer_barrier {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    buffer_barrier.pNext = nullptr;
    buffer_barrier.buffer = vk_buffer;
    buffer_barrier.offset = 0;
    buffer_barrier.size = buffer.GetBufferDesc().width;
    
    buffer_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    buffer_barrier.srcAccessMask = GetAccessFlagFromResourceState(before_state);
    buffer_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    buffer_barrier.dstAccessMask = GetAccessFlagFromResourceState(after_state);

    VkDependencyInfo dep_info {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext = nullptr;
    dep_info.bufferMemoryBarrierCount = 1;
    dep_info.pBufferMemoryBarriers = &buffer_barrier;
    
    vkCmdPipelineBarrier2(vk_command_list, &dep_info);
    
    return true;
}

bool VulkanUtils::AddTextureBarrierToCommandList(IRHICommandList& command_list, IRHITexture& texture,
                                                 RHIResourceStateType before_state, RHIResourceStateType after_state)
{
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto vk_image = dynamic_cast<const VKTexture&>(texture).GetRawImage();
    
    VkImageMemoryBarrier2 image_barrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    image_barrier.pNext = nullptr;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = GetAccessFlagFromResourceState(before_state);
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask = GetAccessFlagFromResourceState(after_state);

    image_barrier.oldLayout = VKConverterUtils::ConvertToImageLayout(before_state);
    //image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VKConverterUtils::ConvertToImageLayout(after_state);

    VkImageAspectFlags aspect_flags = (texture.GetTextureDesc().GetUsage() & RUF_ALLOW_DEPTH_STENCIL) ?
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

bool VulkanUtils::AddUAVBarrier(IRHICommandList& command_list, IRHITexture& texture)
{
    // TODO: Fixup logic
    auto vk_command_list = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto vk_image = dynamic_cast<const VKTexture&>(texture).GetRawImage();
    texture.Transition(command_list, RHIResourceStateType::STATE_RENDER_TARGET);
    
    VkImageMemoryBarrier image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image = vk_image;
    image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = 1;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;
    image_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // Pass 1 写入
    image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;  // Pass 2 读取+写入

    vkCmdPipelineBarrier(
    vk_command_list,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Pass 1 计算阶段
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Pass 2 计算阶段
    0,
    0, nullptr,
    0, nullptr,
    1, &image_barrier);
    
    texture.Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    
    return true;
}

bool VulkanUtils::DrawInstanced(IRHICommandList& command_list, unsigned vertex_count_per_instance, unsigned instance_count,
                                unsigned start_vertex_location, unsigned start_instance_location)
{
    auto command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    vkCmdDraw(command_buffer, vertex_count_per_instance, instance_count, start_vertex_location, start_instance_location);
    return true;
}

bool VulkanUtils::DrawIndexInstanced(IRHICommandList& command_list, unsigned index_count_per_instance,
    unsigned instance_count, unsigned start_index_location, unsigned base_vertex_location, unsigned start_instance_location)
{
    auto command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    vkCmdDrawIndexed(command_buffer, index_count_per_instance, instance_count, start_index_location, base_vertex_location, start_instance_location);
    return true;
}

bool VulkanUtils::Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z)
{
    auto command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    vkCmdDispatch(command_buffer, X, Y, Z);
    return true;
}

bool VulkanUtils::TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y,
    unsigned Z)
{
    return true;
}

bool VulkanUtils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
    unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, unsigned command_stride)
{
    auto command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto indirect_buffer = dynamic_cast<VKBuffer&>(arguments_buffer).GetRawBuffer();
    
    vkCmdDrawIndexedIndirect(command_buffer, indirect_buffer, arguments_buffer_offset, max_count, command_stride);

    return true;
}

bool VulkanUtils::ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature,
                                  unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer,
                                  unsigned count_buffer_offset, unsigned command_stride)
{
    auto command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto indirect_buffer = dynamic_cast<VKBuffer&>(arguments_buffer).GetRawBuffer();
    auto indirect_count_buffer = dynamic_cast<VKBuffer&>(count_buffer).GetRawBuffer();
    
    vkCmdDrawIndexedIndirectCount(command_buffer, indirect_buffer, arguments_buffer_offset, indirect_count_buffer, 0, max_count, command_stride);

    return true;
}

bool VulkanUtils::CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src, const RHICopyTextureInfo& copy_info)
{
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);

    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto& vk_image_src = dynamic_cast<VKTexture&>(src);
    auto& vk_image_dst = dynamic_cast<VKTexture&>(dst);

    auto copy_width = copy_info.copy_width ? copy_info.copy_width : dst.GetTextureDesc().GetTextureWidth();
    auto copy_height = copy_info.copy_height ? copy_info.copy_height : dst.GetTextureDesc().GetTextureHeight();
    
    VkImageCopy image_copy{};
    image_copy.extent = {copy_width, copy_height, 1};
    image_copy.srcOffset = {0, 0, 0};
    image_copy.dstOffset = {copy_info.dst_x, copy_info.dst_y, 0};
    image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.srcSubresource.layerCount = 1;
    image_copy.srcSubresource.mipLevel = copy_info.src_mip_level;
    image_copy.srcSubresource.baseArrayLayer = 0;
    image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.dstSubresource.layerCount = 1;
    image_copy.dstSubresource.mipLevel = copy_info.dst_mip_level;
    image_copy.dstSubresource.baseArrayLayer = 0;

    VkImageLayout src_layout = VKConverterUtils::ConvertToImageLayout(vk_image_src.GetState());
    VkImageLayout dst_layout = VKConverterUtils::ConvertToImageLayout(vk_image_dst.GetState());
    
    vkCmdCopyImage(vk_command_buffer, vk_image_src.GetRawImage(), src_layout, vk_image_dst.GetRawImage(), dst_layout, 1, &image_copy);
    
    return true;
}

bool VulkanUtils::CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHIBuffer& src,
    const RHICopyTextureInfo& copy_info)
{
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    
    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
    auto vk_buffer = dynamic_cast<VKBuffer&>(src).GetRawBuffer();
    auto vk_image = dynamic_cast<VKTexture&>(dst).GetRawImage();
    auto vk_image_layout = VKConverterUtils::ConvertToImageLayout(dst.GetState());

    auto copy_width = copy_info.copy_width ? copy_info.copy_width : dst.GetTextureDesc().GetTextureWidth();
    auto copy_height = copy_info.copy_height ? copy_info.copy_height : dst.GetTextureDesc().GetTextureHeight();

    auto row_length = copy_info.row_pitch ? copy_info.row_pitch / GetBytePerPixelByFormat(dst.GetTextureFormat()) : 0;
    
    VkBufferImageCopy image_copy_info{};
    image_copy_info.bufferOffset = 0;
    image_copy_info.bufferImageHeight = 0;
    image_copy_info.bufferRowLength = row_length;
    image_copy_info.imageOffset = VkOffset3D(copy_info.dst_x, copy_info.dst_y, 0.0f);
    image_copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy_info.imageSubresource.layerCount = 1;
    image_copy_info.imageSubresource.mipLevel = copy_info.dst_mip_level;
    image_copy_info.imageSubresource.baseArrayLayer = 0;
    image_copy_info.imageExtent = VkExtent3D(copy_width, copy_height, 1);
    vkCmdCopyBufferToImage(vk_command_buffer, vk_buffer, vk_image, vk_image_layout, 1, &image_copy_info);
    
    return true;
}

bool VulkanUtils::CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src,
                             size_t src_offset, size_t size)
{
    dst.Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    src.Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);

    auto vk_command_buffer = dynamic_cast<VKCommandList&>(command_list).GetRawCommandBuffer();
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

bool VulkanUtils::ClearUAVTexture(IRHICommandList& command_list, const IRHITextureDescriptorAllocation& texture_descriptor)
{
    VKCommandList& vk_command_list = dynamic_cast<VKCommandList&>(command_list);
    VkCommandBuffer vk_command_buffer = vk_command_list.GetRawCommandBuffer();

    const VKTextureDescriptorAllocation& descriptor_allocation = dynamic_cast<const VKTextureDescriptorAllocation&>(texture_descriptor);
    VkImage vk_texture = dynamic_cast<const VKTexture&>(*descriptor_allocation.m_source).GetRawImage();

    descriptor_allocation.m_source->Transition(command_list, RHIResourceStateType::STATE_COMMON);
    
    VkClearColorValue clearColor = {{ 0.0f, 0.0f, 0.0f, 0.0f }};
    
    VkImageSubresourceRange clearRange{};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = 1;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = 1;
    
    vkCmdClearColorImage(
        vk_command_buffer,
        vk_texture,
        VK_IMAGE_LAYOUT_GENERAL,
        &clearColor,
        1, &clearRange
    );
    
    return true;
}

bool VulkanUtils::SupportRayTracing(IRHIDevice& device)
{
    return false;
}

unsigned VulkanUtils::GetAlignmentSizeForUAVCount(unsigned size)
{
    const UINT alignment = 32;
    return (size + (alignment - 1)) & ~(alignment - 1);
    return 0;
}

void VulkanUtils::ReportLiveObjects()
{
    
}

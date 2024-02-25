#include "VKRenderPass.h"

#include "VKConverterUtils.h"
#include "VKDevice.h"

bool VKRenderPass::InitRenderPass(IRHIDevice& device, const RHIRenderPassInfo& info)
{
    VkRenderPassCreateInfo create_render_pass_info{};
    create_render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    
    std::vector<VkAttachmentDescription> color_attachments(info.attachments.size());
    for (size_t i = 0; i < info.attachments.size(); ++i)
    {
        const auto& attachment = info.attachments[i]; 
        color_attachments[i].format = VKConverterUtils::ConvertToFormat(attachment.format);
        color_attachments[i].samples = VKConverterUtils::ConvertToSampleCount(attachment.sample_count);
        color_attachments[i].loadOp = VKConverterUtils::ConvertToLoadOp(attachment.load_op);
        color_attachments[i].storeOp = VKConverterUtils::ConvertToStoreOp(attachment.store_op);
        color_attachments[i].stencilLoadOp = VKConverterUtils::ConvertToLoadOp(attachment.stencil_load_op);
        color_attachments[i].stencilStoreOp = VKConverterUtils::ConvertToStoreOp(attachment.stencil_store_op);
        color_attachments[i].initialLayout = VKConverterUtils::ConvertToImageLayout(attachment.initial_layout);
        color_attachments[i].finalLayout = VKConverterUtils::ConvertToImageLayout(attachment.final_layout);
    }
    
    create_render_pass_info.attachmentCount = color_attachments.size();
    create_render_pass_info.pAttachments = color_attachments.data();

    std::vector<VkSubpassDescription> subpass_descriptions(info.subpass_infos.size());
    std::vector<std::vector<VkAttachmentReference>> subpass_attachment_referenceses(info.subpass_infos.size());
    for (size_t i = 0; i < info.subpass_infos.size(); ++i)
    {
        const auto& subpass = info.subpass_infos[i];
        
        auto& subpass_attachment_reference = subpass_attachment_referenceses[i]; 
        subpass_attachment_reference.resize(subpass.color_attachments.size());
        for (size_t j = 0; j < subpass.color_attachments.size(); ++j)
        {
            subpass_attachment_reference[j].attachment = subpass.color_attachments[j].attachment_index;
            subpass_attachment_reference[j].layout = VKConverterUtils::ConvertToImageLayout(subpass.color_attachments[j].attachment_layout);
        }
        
        subpass_descriptions[i].pipelineBindPoint = VKConverterUtils::ConvertToBindPoint(subpass.bind_point);
        subpass_descriptions[i].colorAttachmentCount = subpass_attachment_reference.size();
        subpass_descriptions[i].pColorAttachments = subpass_attachment_reference.data();
    }
    create_render_pass_info.subpassCount = subpass_descriptions.size();
    create_render_pass_info.pSubpasses = subpass_descriptions.data();

    std::vector<VkSubpassDependency> subpass_dependencies(info.sub_pass_dependencies.size());
    for (size_t i = 0; i < info.sub_pass_dependencies.size(); ++i)
    {
        const auto& dependency = info.sub_pass_dependencies[i]; 
        subpass_dependencies[i].srcSubpass = dependency.src_subpass;
        subpass_dependencies[i].dstSubpass = dependency.dst_subpass;
        subpass_dependencies[i].srcStageMask = VKConverterUtils::ConvertToPipelineStage(dependency.src_stage);
        subpass_dependencies[i].dstStageMask = VKConverterUtils::ConvertToPipelineStage(dependency.dst_stage);
        subpass_dependencies[i].srcAccessMask = VKConverterUtils::ConvertToAccessFlags(dependency.src_access_flags);
        subpass_dependencies[i].dstAccessMask = VKConverterUtils::ConvertToAccessFlags(dependency.dst_access_flags);
    }
    create_render_pass_info.dependencyCount = subpass_dependencies.size();
    create_render_pass_info.pDependencies = subpass_dependencies.data();

    VkResult result = vkCreateRenderPass(dynamic_cast<VKDevice&>(device).GetDevice(), &create_render_pass_info, nullptr, &m_render_pass);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

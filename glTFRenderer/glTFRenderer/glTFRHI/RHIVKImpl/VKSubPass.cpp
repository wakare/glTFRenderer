#include "VKSubPass.h"
#include "VolkUtils.h"
#include "VKConverterUtils.h"

VKSubPass::VKSubPass()
    : m_subpass_description({})
{
}

bool VKSubPass::InitSubPass(const RHISubPassInfo& sub_pass_info)
{
    m_subpass_description.pipelineBindPoint = VKConverterUtils::ConvertToBindPoint(sub_pass_info.bind_point);

    std::vector<VkAttachmentReference> color_attachments(sub_pass_info.color_attachments.size());

    for (size_t i = 0; i < color_attachments.size(); ++i)
    {
        color_attachments[i].attachment = sub_pass_info.color_attachments[i].attachment_index;
        color_attachments[i].layout = VKConverterUtils::ConvertToImageLayout(sub_pass_info.color_attachments[i].attachment_layout);
    }
    
    m_subpass_description.colorAttachmentCount = color_attachments.size();
    m_subpass_description.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data();
    
    return true;
}

bool VKSubPass::Release(glTFRenderResourceManager&)
{
    return true;
}

VkSubpassDescription VKSubPass::GetSubPassDescription() const
{
    return m_subpass_description;
}

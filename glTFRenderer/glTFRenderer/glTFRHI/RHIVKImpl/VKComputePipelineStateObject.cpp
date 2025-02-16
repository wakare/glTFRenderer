#include "VKComputePipelineStateObject.h"

#include "VKDevice.h"
#include "VKRootSignature.h"
#include "VolkUtils.h"

VkShaderModule VKComputePipelineStateObject::CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries)
{
    VkShaderModuleCreateInfo create_shader_module_info{};
    create_shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_shader_module_info.codeSize = shader_binaries.size();
    create_shader_module_info.pCode = (unsigned*)shader_binaries.data();

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(device, &create_shader_module_info, nullptr, &shader_module);
    GLTF_CHECK(result == VK_SUCCESS);

    return shader_module;
}

VKComputePipelineStateObject::~VKComputePipelineStateObject()
{
    vkDestroyShaderModule(m_device, m_compute_shader_module, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}

bool VKComputePipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
                                                           const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    VkComputePipelineCreateInfo compute_pipeline_create_info{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    
    // Create shader module
    THROW_IF_FAILED(CompileShaders());

    m_compute_shader_module = CreateVkShaderModule(m_device, m_shaders[RHIShaderType::Compute]->GetShaderByteCode());
    
    VkPipelineShaderStageCreateInfo create_compute_stage_info{};
    create_compute_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_compute_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    create_compute_stage_info.module = m_compute_shader_module;
    create_compute_stage_info.pName = m_shaders[RHIShaderType::Compute]->GetMainEntry().c_str();

    compute_pipeline_create_info.stage = create_compute_stage_info;
    
    auto& vk_root_signature = dynamic_cast<const VKRootSignature&>(root_signature);
    const std::vector<VkDescriptorSetLayout>& layouts = vk_root_signature.GetDescriptorSetLayouts();
    
    VkPipelineLayoutCreateInfo create_pipeline_layout_info {};
    create_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_pipeline_layout_info.setLayoutCount = layouts.size();
    create_pipeline_layout_info.pSetLayouts = layouts.data();
    create_pipeline_layout_info.pushConstantRangeCount = 0;
    create_pipeline_layout_info.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(m_device, &create_pipeline_layout_info, nullptr, &m_pipeline_layout);
    GLTF_CHECK(result == VK_SUCCESS);
    compute_pipeline_create_info.layout = m_pipeline_layout;
    
    compute_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    compute_pipeline_create_info.basePipelineIndex = -1;

    result = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &m_pipeline);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

VkPipeline VKComputePipelineStateObject::GetPipeline() const
{
    return m_pipeline;
}

VkPipelineLayout VKComputePipelineStateObject::GetPipelineLayout() const
{
    return m_pipeline_layout;
}

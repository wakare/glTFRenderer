#include "VKPipelineStateObject.h"

#include "VKDevice.h"
#include "VKRenderPass.h"
#include "VKRootSignature.h"
#include "VKSwapChain.h"
#include "ShaderUtil/glTFShaderUtils.h"

VkShaderModule CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries)
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

VKGraphicsPipelineStateObject::~VKGraphicsPipelineStateObject()
{
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}

bool VKGraphicsPipelineStateObject::InitPipelineStateObject(IRHIDevice& device, const RHIPipelineStateInfo& pipeline_state_info)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    // Create shader module
    THROW_IF_FAILED(CompileShaders());

    VkShaderModule vertex_shader_module = CreateVkShaderModule(m_device, m_shaders[RHIShaderType::Vertex]->GetShaderByteCode());
    VkShaderModule fragment_shader_module = CreateVkShaderModule(m_device, m_shaders[RHIShaderType::Pixel]->GetShaderByteCode());

    VkPipelineShaderStageCreateInfo create_vertex_stage_info{};
    create_vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    create_vertex_stage_info.module = vertex_shader_module;
    create_vertex_stage_info.pName = m_shaders[RHIShaderType::Vertex]->GetMainEntry().c_str();

    VkPipelineShaderStageCreateInfo create_frag_stage_info{};
    create_frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    create_frag_stage_info.module = fragment_shader_module;
    create_frag_stage_info.pName = m_shaders[RHIShaderType::Pixel]->GetMainEntry().c_str();

    VkPipelineShaderStageCreateInfo shader_stages[] = {create_vertex_stage_info, create_frag_stage_info}; 

    VkPipelineVertexInputStateCreateInfo create_vertex_input_state_info {};
    create_vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    create_vertex_input_state_info.vertexBindingDescriptionCount = 0;
    create_vertex_input_state_info.pVertexBindingDescriptions = nullptr;
    create_vertex_input_state_info.vertexAttributeDescriptionCount = 0;
    create_vertex_input_state_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo create_input_assembly_info {};
    create_input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    create_input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    create_input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D{dynamic_cast<VKSwapChain&>(pipeline_state_info.m_swap_chain).GetWidth(),
        dynamic_cast<VKSwapChain&>(pipeline_state_info.m_swap_chain).GetHeight()};
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) scissor.extent.width;
    viewport.height = (float) scissor.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    std::vector<VkDynamicState> dynamic_states =
        {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
        };

    VkPipelineDynamicStateCreateInfo create_dynamic_state_info {};
    create_dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    create_dynamic_state_info.dynamicStateCount = static_cast<unsigned>(dynamic_states.size());
    create_dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineViewportStateCreateInfo create_viewport_state_info {};
    create_viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    create_viewport_state_info.viewportCount = 1;
    create_viewport_state_info.scissorCount = 1;
    // dynamic state can switch viewport and scissor during draw commands, so no need to fill viewport and scissor now
    create_viewport_state_info.pViewports = &viewport;
    create_viewport_state_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo create_rasterizer_info{};
    create_rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    create_rasterizer_info.depthClampEnable = VK_FALSE;
    create_rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    create_rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    create_rasterizer_info.lineWidth = 1.0f;
    create_rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    create_rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    create_rasterizer_info.depthBiasEnable = VK_FALSE;
    create_rasterizer_info.depthBiasConstantFactor = 0.0f;
    create_rasterizer_info.depthBiasClamp = 0.0f;
    create_rasterizer_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo create_msaa_state {};
    create_msaa_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    create_msaa_state.sampleShadingEnable = VK_FALSE;
    create_msaa_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    create_msaa_state.minSampleShading = 1.0f;
    create_msaa_state.pSampleMask = nullptr;
    create_msaa_state.alphaToCoverageEnable = VK_FALSE;
    create_msaa_state.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo create_color_blend_state_info {};
    create_color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    create_color_blend_state_info.logicOpEnable = VK_FALSE;
    create_color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
    create_color_blend_state_info.attachmentCount = 1;
    create_color_blend_state_info.pAttachments = &color_blend_attachment_state;
    create_color_blend_state_info.blendConstants[0] = 0.0f;
    create_color_blend_state_info.blendConstants[1] = 0.0f;
    create_color_blend_state_info.blendConstants[2] = 0.0f;
    create_color_blend_state_info.blendConstants[3] = 0.0f;

    VkDescriptorSet descriptor_set = dynamic_cast<const VKRootSignature&>(pipeline_state_info.m_root_signature).GetDescriptorSet();
    
    VkPipelineLayoutCreateInfo create_pipeline_layout_info {};
    create_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_pipeline_layout_info.setLayoutCount = 0;
    create_pipeline_layout_info.pSetLayouts = nullptr;
    create_pipeline_layout_info.pushConstantRangeCount = 0;
    create_pipeline_layout_info.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(m_device, &create_pipeline_layout_info, nullptr, &m_pipeline_layout);
    GLTF_CHECK(result == VK_SUCCESS);

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo create_graphics_pipeline_info {};
    create_graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_graphics_pipeline_info.stageCount = 2;
    create_graphics_pipeline_info.pStages = shader_stages;
    create_graphics_pipeline_info.pVertexInputState = &create_vertex_input_state_info;
    create_graphics_pipeline_info.pInputAssemblyState = &create_input_assembly_info;
    create_graphics_pipeline_info.pViewportState = &create_viewport_state_info;
    create_graphics_pipeline_info.pRasterizationState = &create_rasterizer_info;
    create_graphics_pipeline_info.pMultisampleState = &create_msaa_state;
    create_graphics_pipeline_info.pDepthStencilState = nullptr;
    create_graphics_pipeline_info.pColorBlendState = &create_color_blend_state_info;
    create_graphics_pipeline_info.pDynamicState = &create_dynamic_state_info;
    create_graphics_pipeline_info.layout = m_pipeline_layout;
    create_graphics_pipeline_info.renderPass = dynamic_cast<VKRenderPass&>(pipeline_state_info.m_render_pass).GetRenderPass();
    create_graphics_pipeline_info.subpass = 0;
    create_graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    create_graphics_pipeline_info.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &create_graphics_pipeline_info, nullptr, &m_pipeline);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

bool VKGraphicsPipelineStateObject::BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets)
{
    return true;
}

bool VKGraphicsPipelineStateObject::BindRenderTargetFormats(
    const std::vector<IRHIDescriptorAllocation*>& render_targets)
{
    return true;
}

VkPipeline VKGraphicsPipelineStateObject::GetPipeline() const
{
    return m_pipeline;
}

VkPipelineLayout VKGraphicsPipelineStateObject::GetPipelineLayout() const
{
    return m_pipeline_layout;
}

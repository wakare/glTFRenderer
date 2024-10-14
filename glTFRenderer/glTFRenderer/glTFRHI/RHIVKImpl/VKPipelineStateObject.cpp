#include "VKPipelineStateObject.h"

#include "VKConverterUtils.h"
#include "VKDevice.h"
#include "VKRootSignature.h"
#include "VKSwapChain.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorManager.h"
#include "ShaderUtil/glTFShaderUtils.h"

VkShaderModule VKGraphicsPipelineStateObject::CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries)
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
    vkDestroyShaderModule(m_device, m_vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_device, m_fragment_shader_module, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
}

bool VKGraphicsPipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
    const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts)
{
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();
    
    // Create shader module
    THROW_IF_FAILED(CompileShaders());

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    
    GLTF_CHECK(m_shaders.contains(RHIShaderType::Vertex));
    m_vertex_shader_module = CreateVkShaderModule(m_device, m_shaders[RHIShaderType::Vertex]->GetShaderByteCode());
    {
        VkPipelineShaderStageCreateInfo create_vertex_stage_info{};
        create_vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        create_vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        create_vertex_stage_info.module = m_vertex_shader_module;
        create_vertex_stage_info.pName = m_shaders[RHIShaderType::Vertex]->GetMainEntry().c_str();
        shader_stages.push_back(create_vertex_stage_info);    
    }

    if (m_shaders.contains(RHIShaderType::Pixel))
    {
        m_fragment_shader_module = CreateVkShaderModule(m_device, m_shaders[RHIShaderType::Pixel]->GetShaderByteCode());
        VkPipelineShaderStageCreateInfo create_frag_stage_info{};
        create_frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        create_frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        create_frag_stage_info.module = m_fragment_shader_module;
        create_frag_stage_info.pName = m_shaders[RHIShaderType::Pixel]->GetMainEntry().c_str();
        shader_stages.push_back(create_frag_stage_info);
    }

    std::map<unsigned, VkVertexInputBindingDescription> binding_records;
    
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    attribute_descriptions.reserve(input_layouts.size());
    for (const auto& input_layout : input_layouts)
    {
        attribute_descriptions.push_back(
            {
                input_layout.layout_location,
                input_layout.slot,
                VKConverterUtils::ConvertToFormat(input_layout.format),
                input_layout.aligned_byte_offset
            });

        binding_records[input_layout.slot].binding = input_layout.slot;
        binding_records[input_layout.slot].inputRate = input_layout.frequency == PER_VERTEX ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        binding_records[input_layout.slot].stride += GetBytePerPixelByFormat(input_layout.format);
    }
    
    std::vector<VkVertexInputBindingDescription> binding_descriptions;
    binding_descriptions.reserve(binding_records.size());
    for (const auto& binding_record : binding_records)
    {
        binding_descriptions.push_back(binding_record.second);
    }
    
    VkPipelineVertexInputStateCreateInfo create_vertex_input_state_info {};
    create_vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    create_vertex_input_state_info.vertexBindingDescriptionCount = binding_descriptions.size();
    create_vertex_input_state_info.pVertexBindingDescriptions = binding_descriptions.data();
    create_vertex_input_state_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
    create_vertex_input_state_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo create_input_assembly_info {};
    create_input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    create_input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    create_input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D{dynamic_cast<VKSwapChain&>(swap_chain).GetWidth(),
        dynamic_cast<VKSwapChain&>(swap_chain).GetHeight()};
    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = scissor.extent.height;
    viewport.width = (float) scissor.extent.width;
    viewport.height = (float) -scissor.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    std::vector<VkDynamicState> dynamic_states =
        {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
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

    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
    for (const auto& bind_render_target : m_bind_render_target_formats)
    {
        VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
        color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_state.blendEnable = VK_FALSE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_states.push_back(color_blend_attachment_state);
    }    

    VkPipelineColorBlendStateCreateInfo create_color_blend_state_info {};
    create_color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    create_color_blend_state_info.logicOpEnable = VK_FALSE;
    create_color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
    create_color_blend_state_info.attachmentCount = color_blend_attachment_states.size();
    create_color_blend_state_info.pAttachments = color_blend_attachment_states.data();
    create_color_blend_state_info.blendConstants[0] = 0.0f;
    create_color_blend_state_info.blendConstants[1] = 0.0f;
    create_color_blend_state_info.blendConstants[2] = 0.0f;
    create_color_blend_state_info.blendConstants[3] = 0.0f;

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

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil_state_create_info.pNext = nullptr;
    depth_stencil_state_create_info.depthTestEnable = true;
    depth_stencil_state_create_info.depthWriteEnable = true;
    depth_stencil_state_create_info.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
    
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipeline_rendering_create_info.colorAttachmentCount = m_bind_render_target_formats.size();
    pipeline_rendering_create_info.pColorAttachmentFormats = m_bind_render_target_formats.data();
    pipeline_rendering_create_info.depthAttachmentFormat = m_bind_depth_stencil_format;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo create_graphics_pipeline_info {};
    create_graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_graphics_pipeline_info.stageCount = shader_stages.size();
    create_graphics_pipeline_info.pStages = shader_stages.data();
    create_graphics_pipeline_info.pVertexInputState = &create_vertex_input_state_info;
    create_graphics_pipeline_info.pInputAssemblyState = &create_input_assembly_info;
    create_graphics_pipeline_info.pViewportState = &create_viewport_state_info;
    create_graphics_pipeline_info.pRasterizationState = &create_rasterizer_info;
    create_graphics_pipeline_info.pMultisampleState = &create_msaa_state;
    create_graphics_pipeline_info.pDepthStencilState = &depth_stencil_state_create_info;
    create_graphics_pipeline_info.pColorBlendState = &create_color_blend_state_info;
    create_graphics_pipeline_info.pDynamicState = &create_dynamic_state_info;
    create_graphics_pipeline_info.layout = m_pipeline_layout;
    create_graphics_pipeline_info.subpass = 0;
    create_graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    create_graphics_pipeline_info.basePipelineIndex = -1;
    create_graphics_pipeline_info.pNext = &pipeline_rendering_create_info;

    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &create_graphics_pipeline_info, nullptr, &m_pipeline);
    GLTF_CHECK(result == VK_SUCCESS);
    
    return true;
}

bool VKGraphicsPipelineStateObject::BindRenderTargetFormats(
    const std::vector<IRHIDescriptorAllocation*>& render_targets)
{
    m_bind_render_target_formats.clear();
    m_bind_depth_stencil_format = VK_FORMAT_UNDEFINED;

    for (const auto& render_target : render_targets)
    {
        if (render_target->GetDesc().m_view_type == RHIViewType::RVT_RTV)
        {
            m_bind_render_target_formats.push_back(VKConverterUtils::ConvertToFormat(render_target->GetDesc().m_format));    
        }
        else if (render_target->GetDesc().m_view_type == RHIViewType::RVT_DSV)
        {
            const RHIDataFormat convert_depth_stencil_format = render_target->GetDesc().m_format == RHIDataFormat::R32_TYPELESS ?
                RHIDataFormat::D32_FLOAT : render_target->GetDesc().m_format;
            m_bind_depth_stencil_format = VKConverterUtils::ConvertToFormat(convert_depth_stencil_format);
        }
        else
        {
            // Not supported render target type!
            assert(false);
        }
    }
    
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
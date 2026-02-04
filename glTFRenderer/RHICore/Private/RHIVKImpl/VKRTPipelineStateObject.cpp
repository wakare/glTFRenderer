#include "VKRTPipelineStateObject.h"

#include "VKDevice.h"
#include "VKRootSignature.h"
#include "RendererCommon.h"
#include "ShaderReflect/spirv_reflect.h"

#include <algorithm>

namespace
{
    VkShaderModule CreateVkShaderModule(VkDevice device, const std::vector<unsigned char>& shader_binaries)
    {
        VkShaderModuleCreateInfo create_shader_module_info{};
        create_shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_shader_module_info.codeSize = shader_binaries.size();
        create_shader_module_info.pCode = reinterpret_cast<const uint32_t*>(shader_binaries.data());

        VkShaderModule shader_module = VK_NULL_HANDLE;
        VkResult result = vkCreateShaderModule(device, &create_shader_module_info, nullptr, &shader_module);
        GLTF_CHECK(result == VK_SUCCESS);
        return shader_module;
    }

    VkShaderStageFlagBits ToVkShaderStage(SpvReflectShaderStageFlagBits stage)
    {
        return static_cast<VkShaderStageFlagBits>(stage);
    }

    bool IsGeneralShaderStage(VkShaderStageFlagBits stage)
    {
        return stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR
            || stage == VK_SHADER_STAGE_MISS_BIT_KHR
            || stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    }
}

bool VKRTPipelineStateObject::InitPipelineStateObject(IRHIDevice& device,
                                                      const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::map<RHIShaderType, std::shared_ptr<
                                                      IRHIShader>>& shaders)
{
    (void)swap_chain;
    
    m_device = dynamic_cast<VKDevice&>(device).GetDevice();

    const auto& bind_rt_shader = *shaders.at(RHIShaderType::RayTracing);
    const auto& shader_binaries = bind_rt_shader.GetShaderByteCode();
    m_shader_module = CreateVkShaderModule(m_device, shader_binaries);

    SpvReflectShaderModule reflect_module;
    SpvReflectResult reflect_result = spvReflectCreateShaderModule(shader_binaries.size(), shader_binaries.data(), &reflect_module);
    GLTF_CHECK(reflect_result == SPV_REFLECT_RESULT_SUCCESS);

    std::unordered_map<std::string, VkShaderStageFlagBits> entry_stage_map;
    for (uint32_t i = 0; i < reflect_module.entry_point_count; ++i)
    {
        const auto& entry = reflect_module.entry_points[i];
        entry_stage_map.insert({ entry.name, ToVkShaderStage(entry.shader_stage) });
    }

    spvReflectDestroyShaderModule(&reflect_module);

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    std::unordered_map<std::string, uint32_t> entry_stage_indices;

    auto add_stage = [&](const std::string& entry_name)
    {
        if (entry_name.empty() || entry_stage_indices.contains(entry_name))
        {
            return;
        }

        auto it = entry_stage_map.find(entry_name);
        GLTF_CHECK(it != entry_stage_map.end());

        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = it->second;
        stage_info.module = m_shader_module;
        stage_info.pName = entry_name.c_str();

        entry_stage_indices[entry_name] = static_cast<uint32_t>(shader_stages.size());
        shader_stages.push_back(stage_info);
    };

    for (const auto& hit_group_desc : m_hit_group_descs)
    {
        add_stage(hit_group_desc.m_closest_hit_entry_name);
        add_stage(hit_group_desc.m_any_hit_entry_name);
        add_stage(hit_group_desc.m_intersection_entry_name);
    }

    for (const auto& export_name : m_export_function_names)
    {
        add_stage(export_name);
    }

    auto add_general_group = [&](const std::string& entry_name)
    {
        if (entry_name.empty() || m_group_name_to_index.contains(entry_name))
        {
            return;
        }

        const auto stage_it = entry_stage_map.find(entry_name);
        if (stage_it == entry_stage_map.end() || !IsGeneralShaderStage(stage_it->second))
        {
            return;
        }

        VkRayTracingShaderGroupCreateInfoKHR group_info{};
        group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group_info.generalShader = entry_stage_indices.at(entry_name);
        group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
        group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
        group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

        m_group_name_to_index[entry_name] = static_cast<uint32_t>(m_shader_groups.size());
        m_shader_groups.push_back(group_info);
    };

    for (const auto& export_name : m_export_function_names)
    {
        add_general_group(export_name);
    }

    for (const auto& hit_group_desc : m_hit_group_descs)
    {
        VkRayTracingShaderGroupCreateInfoKHR group_info{};
        group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group_info.generalShader = VK_SHADER_UNUSED_KHR;
        group_info.closestHitShader = hit_group_desc.m_closest_hit_entry_name.empty() ? VK_SHADER_UNUSED_KHR : entry_stage_indices.at(hit_group_desc.m_closest_hit_entry_name);
        group_info.anyHitShader = hit_group_desc.m_any_hit_entry_name.empty() ? VK_SHADER_UNUSED_KHR : entry_stage_indices.at(hit_group_desc.m_any_hit_entry_name);
        group_info.intersectionShader = hit_group_desc.m_intersection_entry_name.empty() ? VK_SHADER_UNUSED_KHR : entry_stage_indices.at(hit_group_desc.m_intersection_entry_name);

        m_group_name_to_index[hit_group_desc.m_export_hit_group_name] = static_cast<uint32_t>(m_shader_groups.size());
        m_shader_groups.push_back(group_info);
    }

    const auto& vk_root_signature = dynamic_cast<const VKRootSignature&>(root_signature);
    const auto& layouts = vk_root_signature.GetDescriptorSetLayouts();

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pipeline_layout_info.pSetLayouts = layouts.data();
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    GLTF_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout) == VK_SUCCESS);

    VkRayTracingPipelineCreateInfoKHR pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(m_shader_groups.size());
    pipeline_info.pGroups = m_shader_groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = std::max(1u, m_config.max_recursion_count);
    pipeline_info.layout = m_pipeline_layout;

    GLTF_CHECK(vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) == VK_SUCCESS);

    need_release = true;
    return true;
}

bool VKRTPipelineStateObject::Release(IRHIMemoryManager& memory_manager)
{
    (void)memory_manager;
    if (!need_release)
    {
        return true;
    }

    need_release = false;
    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
    if (m_shader_module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_shader_module, nullptr);
        m_shader_module = VK_NULL_HANDLE;
    }

    return true;
}

uint32_t VKRTPipelineStateObject::GetShaderGroupIndex(const std::string& name) const
{
    auto it = m_group_name_to_index.find(name);
    GLTF_CHECK(it != m_group_name_to_index.end());
    return it->second;
}

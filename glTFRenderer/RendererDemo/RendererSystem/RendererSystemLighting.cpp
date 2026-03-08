// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "RendererSystemLighting.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <imgui/imgui.h>

#include "RendererSceneAABB.h"
#include "../../RHICore/Public/RHIConfigSingleton.h"

namespace
{
    bool UseDx12ShadowBufferingStrategy()
    {
        return RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12;
    }
}

RendererSystemLighting::RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator,
                                               std::shared_ptr<RendererSystemSceneRenderer> scene)
    : m_scene(std::move(scene))
{
    m_lighting_module = std::make_shared<RendererModuleLighting>(resource_operator);
    m_modules.push_back(m_lighting_module);
}

unsigned RendererSystemLighting::AddLight(const LightInfo& light_info)
{
    GLTF_CHECK(m_lighting_module);
    return m_lighting_module->AddLightInfo(light_info);
}

bool RendererSystemLighting::UpdateLight(unsigned index, const LightInfo& light_info)
{
    GLTF_CHECK(m_lighting_module);
    GLTF_CHECK(m_lighting_module->ContainsLight(index));
    return m_lighting_module->UpdateLightInfo(index, light_info);
}

bool RendererSystemLighting::GetDominantDirectionalLight(glm::fvec3& out_direction, float& out_luminance) const
{
    out_direction = {0.0f, -1.0f, 0.0f};
    out_luminance = 0.0f;
    if (!m_lighting_module)
    {
        return false;
    }

    const auto& lights = m_lighting_module->GetLightInfos();
    float strongest_luminance = -1.0f;
    bool found_directional_light = false;
    for (const auto& light_info : lights)
    {
        if (light_info.type != LightType::Directional)
        {
            continue;
        }

        const glm::fvec3 intensity = {
            (std::max)(0.0f, light_info.intensity.x),
            (std::max)(0.0f, light_info.intensity.y),
            (std::max)(0.0f, light_info.intensity.z)
        };
        const float luminance = glm::dot(intensity, glm::fvec3(0.2126f, 0.7152f, 0.0722f));
        if (!found_directional_light || luminance > strongest_luminance)
        {
            found_directional_light = true;
            strongest_luminance = luminance;
            out_direction = light_info.position;
        }
    }

    if (!found_directional_light)
    {
        return false;
    }

    const float direction_len_sq = glm::dot(out_direction, out_direction);
    if (direction_len_sq <= 1e-8f)
    {
        return false;
    }

    out_direction *= 1.0f / std::sqrt(direction_len_sq);
    out_luminance = (std::max)(0.0f, strongest_luminance);
    return true;
}

bool RendererSystemLighting::CastShadow() const
{
    return m_cast_shadow;
}

void RendererSystemLighting::SetCastShadow(bool cast_shadow)
{
    m_cast_shadow = cast_shadow;
}

RendererInterface::RenderTargetHandle RendererSystemLighting::GetLightingOutput() const
{
    return m_lighting_pass_output;
}

bool RendererSystemLighting::Init(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene->HasInit());
    //const bool use_dx12_shadow_buffering = UseDx12ShadowBufferingStrategy();
    //const bool use_dx12_shadow_buffering = true;
    
    m_lighting_pass_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget("LightingPass_Output", RendererInterface::RGBA16_FLOAT, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::UNORDER_ACCESS | RendererInterface::ResourceUsage::SHADER_RESOURCE));
    
    auto m_camera_module = m_scene->GetCameraModule();
    auto width = m_camera_module->GetWidth();
    auto height = m_camera_module->GetHeight();

    // Directional light shadow pass
    const auto& lights = m_lighting_module->GetLightInfos();
    for (unsigned i = 0; i < lights.size(); ++i)
    {
        const auto& light = lights[i];
        if (light.type != LightType::Directional)
        {
            continue;
        }

        auto& new_shadow_pass_resource = m_shadow_pass_resources[i];
        const std::string shadowmap_name = std::format("directional_shadowmap_{}", i);

        const unsigned shadowmap_width = 1024;
        const unsigned shadowmap_height = 1024;
        RendererInterface::RenderTargetDesc shadowmap_desc{};
        shadowmap_desc.name = shadowmap_name;
        shadowmap_desc.format = RendererInterface::D32;
        shadowmap_desc.width = shadowmap_width;
        shadowmap_desc.height = shadowmap_height;
        shadowmap_desc.clear = RendererInterface::default_clear_depth;
        shadowmap_desc.usage = static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::DEPTH_STENCIL |
            RendererInterface::ResourceUsage::SHADER_RESOURCE);
        new_shadow_pass_resource.m_shadow_maps =
            resource_operator.CreateFrameBufferedRenderTargets(shadowmap_desc, shadowmap_name);
        GLTF_CHECK(!new_shadow_pass_resource.m_shadow_maps.empty());
        new_shadow_pass_resource.m_bound_shadow_map = new_shadow_pass_resource.m_shadow_maps[0];

        ShadowPassResource::CalcDirectionalLightShadowMatrix(light, m_scene->GetSceneMeshModule()->GetSceneBounds(),
            0.0f, 0.0f, 1.0f, 1.0f, shadowmap_width, shadowmap_height, new_shadow_pass_resource.m_shadow_map_view_buffer, new_shadow_pass_resource.m_shadow_map_info);
        RendererInterface::BufferDesc camera_buffer_desc{};
        camera_buffer_desc.name = "ViewBuffer";
        camera_buffer_desc.size = sizeof(ViewBuffer);
        //camera_buffer_desc.type = use_dx12_shadow_buffering ? RendererInterface::DEFAULT : RendererInterface::UPLOAD;
        camera_buffer_desc.type =  RendererInterface::UPLOAD;
        camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
        camera_buffer_desc.data = &new_shadow_pass_resource.m_shadow_map_view_buffer;
        new_shadow_pass_resource.m_shadow_map_buffer_handles =
            resource_operator.CreateFrameBufferedBuffers(
                camera_buffer_desc,
                std::format("ViewBuffer_shadow_{}", i));
        new_shadow_pass_resource.m_shadow_map_view_buffers.assign(
            new_shadow_pass_resource.m_shadow_map_buffer_handles.size(),
            new_shadow_pass_resource.m_shadow_map_view_buffer);

        RendererInterface::RenderGraph::RenderPassSetupInfo shadow_pass_setup_info{};
        shadow_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
        shadow_pass_setup_info.debug_group = "Lighting";
        shadow_pass_setup_info.debug_name = std::format("Directional Shadow {}", i);
        shadow_pass_setup_info.modules = {m_scene->GetSceneMeshModule()};
        shadow_pass_setup_info.excluded_buffer_bindings.insert("g_material_infos");
        shadow_pass_setup_info.excluded_texture_bindings.insert("bindless_material_textures");
        shadow_pass_setup_info.shader_setup_infos = {
            {
                .shader_type = RendererInterface::ShaderType::VERTEX_SHADER,
                .entry_function = "MainVS",
                .shader_file = "Resources/Shaders/ModelRenderingShader.hlsl"
            }
        };

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;

        shadow_pass_setup_info.render_targets = {
            {new_shadow_pass_resource.m_bound_shadow_map, depth_binding_desc}
        };
        
        RendererInterface::BufferBindingDesc camera_binding_desc{};
        camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
        camera_binding_desc.buffer_handle = new_shadow_pass_resource.m_shadow_map_buffer_handles[0];
        camera_binding_desc.is_structured_buffer = false;
        shadow_pass_setup_info.buffer_resources["ViewBuffer"] = camera_binding_desc;

        shadow_pass_setup_info.viewport_width = shadowmap_width;
        shadow_pass_setup_info.viewport_height = shadowmap_height;
        new_shadow_pass_resource.m_shadow_pass_node = graph.CreateRenderGraphNode(resource_operator, shadow_pass_setup_info);
    
        GLTF_CHECK(!new_shadow_pass_resource.m_shadow_map_buffer_handles.empty());
    }
    
    RendererInterface::RenderGraph::RenderPassSetupInfo lighting_pass_setup_info{};
    lighting_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    lighting_pass_setup_info.debug_group = "Lighting";
    lighting_pass_setup_info.debug_name = "Scene Lighting";
    lighting_pass_setup_info.modules = {m_lighting_module, m_camera_module};
    lighting_pass_setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/SceneLighting.hlsl"}
    };
    RendererSystemOutput<RendererSystemSceneRenderer> output;
    
    RendererInterface::RenderTargetTextureBindingDesc albedo_binding_desc{};
    albedo_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    albedo_binding_desc.name = "albedoTex";
    albedo_binding_desc.render_target_texture = {output.GetRenderTargetHandle(*m_scene, "m_base_pass_color")};
        
    RendererInterface::RenderTargetTextureBindingDesc normal_binding_desc{};
    normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    normal_binding_desc.name = "normalTex";
    normal_binding_desc.render_target_texture = {output.GetRenderTargetHandle(*m_scene, "m_base_pass_normal")};
        
    RendererInterface::RenderTargetTextureBindingDesc depth_binding_desc{};
    depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    depth_binding_desc.name = "depthTex";
    depth_binding_desc.render_target_texture = {output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth")};

    RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
    output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
    output_binding_desc.name = "Output";
    output_binding_desc.render_target_texture = {m_lighting_pass_output};

    RendererInterface::RenderTargetTextureBindingDesc shadowmap_binding_desc{};
    shadowmap_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    shadowmap_binding_desc.name = "bindless_shadowmap_textures";
    for (const auto& shadow_resource : m_shadow_pass_resources)
    {
        shadowmap_binding_desc.render_target_texture.push_back(shadow_resource.second.m_bound_shadow_map);
    }
    
    lighting_pass_setup_info.sampled_render_targets = {
        albedo_binding_desc,
        normal_binding_desc,
        depth_binding_desc,
        shadowmap_binding_desc,
        output_binding_desc
    };

    std::vector<ShadowMapInfo> shadowmap_infos;
    for (const auto& shadow_resource : m_shadow_pass_resources)
    {
        shadowmap_infos.push_back(shadow_resource.second.m_shadow_map_info);
    }
    RendererInterface::BufferDesc shadowmap_info_buffer_desc{};
    shadowmap_info_buffer_desc.type = RendererInterface::DEFAULT;
    shadowmap_info_buffer_desc.name = "g_shadowmap_infos";
    shadowmap_info_buffer_desc.size = sizeof(ShadowMapInfo) * shadowmap_infos.size();
    shadowmap_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    shadowmap_info_buffer_desc.data = shadowmap_infos.data();
    //m_lighting_pass_shadow_infos_handles =
    //    resource_operator.CreateFrameBufferedBuffers(shadowmap_info_buffer_desc, "g_shadowmap_infos");
    m_lighting_pass_shadow_infos_handles = {resource_operator.CreateBuffer(shadowmap_info_buffer_desc)};
    
    RendererInterface::BufferBindingDesc shadowmap_info_binding_desc{};
    shadowmap_info_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    GLTF_CHECK(!m_lighting_pass_shadow_infos_handles.empty());
    shadowmap_info_binding_desc.buffer_handle = m_lighting_pass_shadow_infos_handles.front();
    shadowmap_info_binding_desc.is_structured_buffer = true;
    shadowmap_info_binding_desc.stride = sizeof(ShadowMapInfo);
    shadowmap_info_binding_desc.count = shadowmap_infos.size();
    lighting_pass_setup_info.buffer_resources["g_shadowmap_infos"] = shadowmap_info_binding_desc;
    
    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = (width + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = (height + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    lighting_pass_setup_info.execute_command = dispatch_command;
    lighting_pass_setup_info.dependency_render_graph_nodes.reserve(m_shadow_pass_resources.size());
    for (const auto& shadow_resource : m_shadow_pass_resources)
    {
        if (shadow_resource.second.m_shadow_pass_node != NULL_HANDLE)
        {
            lighting_pass_setup_info.dependency_render_graph_nodes.push_back(shadow_resource.second.m_shadow_pass_node);
        }
    }

    m_lighting_pass_node = graph.CreateRenderGraphNode(resource_operator, lighting_pass_setup_info);

    graph.RegisterRenderTargetToColorOutput(m_lighting_pass_output);
    
    return true;
}

bool RendererSystemLighting::HasInit() const
{
    return m_lighting_pass_node != NULL_HANDLE;
}

void RendererSystemLighting::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    std::vector<LightInfo> cached_lights;
    if (m_lighting_module)
    {
        cached_lights = m_lighting_module->GetLightInfos();
    }

    m_lighting_module = std::make_shared<RendererModuleLighting>(resource_operator);
    for (const auto& light_info : cached_lights)
    {
        m_lighting_module->AddLightInfo(light_info);
    }

    m_modules.clear();
    m_modules.push_back(m_lighting_module);
    m_shadow_pass_resources.clear();
    m_lighting_pass_node = NULL_HANDLE;
    m_lighting_pass_output = NULL_HANDLE;
    m_lighting_pass_shadow_infos_handles.clear();
}

bool RendererSystemLighting::Tick(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    const unsigned width = m_scene->GetWidth();
    const unsigned height = m_scene->GetHeight();
    graph.UpdateComputeDispatch(m_lighting_pass_node, (width + 7) / 8, (height + 7) / 8, 1);

    if (!m_lighting_module->m_light_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_node,
            "g_lightInfos",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_module->m_light_buffer_handles));
    }
    if (!m_lighting_module->m_light_count_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_node,
            "LightInfoConstantBuffer",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_module->m_light_count_buffer_handles));
    }
    if (!m_lighting_pass_shadow_infos_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_node,
            "g_shadowmap_infos",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_pass_shadow_infos_handles));
    }

    if (CastShadow())
    {
        UpdateDirectionalShadowResources(resource_operator);
        std::vector<RendererInterface::RenderTargetHandle> current_shadow_maps;
        current_shadow_maps.reserve(m_shadow_pass_resources.size());
        for (auto& shadow_pass: m_shadow_pass_resources)
        {
            auto& shadow_resource = shadow_pass.second;
            if (!shadow_resource.m_shadow_maps.empty())
            {
                const auto current_shadow_map =
                    resource_operator.GetFrameBufferedRenderTargetHandle(shadow_resource.m_shadow_maps);
                if (shadow_resource.m_bound_shadow_map == NULL_HANDLE)
                {
                    shadow_resource.m_bound_shadow_map = current_shadow_map;
                }
                else if (shadow_resource.m_bound_shadow_map != current_shadow_map)
                {
                    graph.UpdateNodeRenderTargetBinding(
                        shadow_resource.m_shadow_pass_node,
                        shadow_resource.m_bound_shadow_map,
                        current_shadow_map);
                    shadow_resource.m_bound_shadow_map = current_shadow_map;
                }

                current_shadow_maps.push_back(shadow_resource.m_bound_shadow_map);
            }

            const auto& shadow_buffers = shadow_pass.second.m_shadow_map_buffer_handles;
            if (!shadow_buffers.empty())
            {
                graph.UpdateNodeBufferBinding(
                    shadow_pass.second.m_shadow_pass_node,
                    "ViewBuffer",
                    resource_operator.GetFrameBufferedBufferHandle(shadow_buffers));
            }
            graph.RegisterRenderGraphNode(shadow_pass.second.m_shadow_pass_node);
        }

        if (!current_shadow_maps.empty())
        {
            graph.UpdateNodeRenderTargetTextureBinding(
                m_lighting_pass_node,
                "bindless_shadowmap_textures",
                current_shadow_maps);
        }
    }
    
    graph.RegisterRenderGraphNode(m_lighting_pass_node);
    
    m_lighting_module->Tick(resource_operator, interval);
    
    return true;
}

void RendererSystemLighting::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
}

void RendererSystemLighting::DrawDebugUI()
{
    if (!m_lighting_module)
    {
        ImGui::TextUnformatted("Lighting module not initialized.");
        return;
    }

    bool cast_shadow = m_cast_shadow;
    if (ImGui::Checkbox("Cast Shadow", &cast_shadow))
    {
        SetCastShadow(cast_shadow);
    }

    ImGui::Text("Lights: %u", static_cast<unsigned>(m_lighting_module->GetLightInfos().size()));
    ImGui::Text("Directional Shadow Maps: %u", static_cast<unsigned>(m_shadow_pass_resources.size()));
}

void RendererSystemLighting::UpdateDirectionalShadowResources(RendererInterface::ResourceOperator& resource_operator)
{
    if (m_shadow_pass_resources.empty())
    {
        return;
    }

    std::vector<ShadowMapInfo> shadowmap_infos;
    shadowmap_infos.reserve(m_shadow_pass_resources.size());
    const unsigned current_frame_slot_index = resource_operator.GetCurrentFrameSlotIndex();

    const auto& lights = m_lighting_module->GetLightInfos();
    const auto scene_bounds = m_scene->GetSceneMeshModule()->GetSceneBounds();
    for (auto& shadow_resource_pair : m_shadow_pass_resources)
    {
        const unsigned light_index = shadow_resource_pair.first;
        auto& shadow_resource = shadow_resource_pair.second;
        GLTF_CHECK(light_index < lights.size());

        const auto& light_info = lights[light_index];
        GLTF_CHECK(light_info.type == LightType::Directional);
        const unsigned shadowmap_width = shadow_resource.m_shadow_map_info.shadowmap_size[0];
        const unsigned shadowmap_height = shadow_resource.m_shadow_map_info.shadowmap_size[1];
        ShadowPassResource::CalcDirectionalLightShadowMatrix(
            light_info,
            scene_bounds,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            shadowmap_width,
            shadowmap_height,
            shadow_resource.m_shadow_map_view_buffer,
            shadow_resource.m_shadow_map_info);

        if (!shadow_resource.m_shadow_map_buffer_handles.empty() &&
            shadow_resource.m_shadow_map_buffer_handles.size() == shadow_resource.m_shadow_map_view_buffers.size())
        {
            const unsigned frame_slot = current_frame_slot_index % static_cast<unsigned>(shadow_resource.m_shadow_map_buffer_handles.size());
            shadow_resource.m_shadow_map_view_buffers[frame_slot] = shadow_resource.m_shadow_map_view_buffer;

            RendererInterface::BufferUploadDesc shadow_camera_upload_desc{};
            shadow_camera_upload_desc.data = &shadow_resource.m_shadow_map_view_buffers[frame_slot];
            shadow_camera_upload_desc.size = sizeof(ViewBuffer);
            resource_operator.UploadFrameBufferedBufferData(shadow_resource.m_shadow_map_buffer_handles, shadow_camera_upload_desc);
        }
        shadowmap_infos.push_back(shadow_resource.m_shadow_map_info);
    }

    if (!shadowmap_infos.empty() && !m_lighting_pass_shadow_infos_handles.empty())
    {
        RendererInterface::BufferUploadDesc shadow_info_upload_desc{};
        shadow_info_upload_desc.data = shadowmap_infos.data();
        shadow_info_upload_desc.size = shadowmap_infos.size() * sizeof(ShadowMapInfo);
        resource_operator.UploadFrameBufferedBufferData(m_lighting_pass_shadow_infos_handles, shadow_info_upload_desc);
    }
}

bool RendererSystemLighting::ShadowPassResource::CalcDirectionalLightShadowMatrix(
    const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y,
    float ndc_width, float ndc_height, unsigned shadowmap_width, unsigned shadowmap_height,
    ViewBuffer& out_view_buffer, ShadowMapInfo& out_shadow_info)
{
    const auto& scene_bounds_min = scene_bounds.getMin();
    const auto& scene_bounds_max = scene_bounds.getMax();
    const auto& scene_center = scene_bounds.getCenter();
    
    glm::vec3 light_dir = directional_light_info.position;
    const float light_len_sq = glm::dot(light_dir, light_dir);
    if (light_len_sq <= 1e-8f)
    {
        return false;
    }
    light_dir *= 1.0f / std::sqrt(light_len_sq);

    const glm::vec3 scene_extent = scene_bounds_max - scene_bounds_min;
    const float scene_radius = (std::max)(0.5f * glm::length(scene_extent), 1.0f);
    const float shadow_radius = scene_radius * 1.1f;

    glm::vec3 light_up = {0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(light_dir, light_up)) > 0.99f)
    {
        light_up = {0.0f, 0.0f, 1.0f};
    }

    const glm::vec3 light_eye = scene_center - light_dir * shadow_radius;
    glm::mat4x4 view_matrix = glm::lookAtLH(light_eye, scene_center, light_up);

    std::array<glm::vec4, 8> scene_corners = {
        glm::vec4(scene_bounds_min.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_min.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f),
        glm::vec4(scene_bounds_max.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f)};

    glm::vec3 light_space_min{
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()};
    glm::vec3 light_space_max{
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)(),
        (std::numeric_limits<float>::lowest)()};
    for (const auto& scene_corner : scene_corners)
    {
        const glm::vec4 light_space_corner = view_matrix * scene_corner;
        light_space_min = glm::min(light_space_min, glm::vec3(light_space_corner));
        light_space_max = glm::max(light_space_max, glm::vec3(light_space_corner));
    }

    const glm::vec3 light_space_extent = light_space_max - light_space_min;
    const glm::vec3 light_space_padding = glm::max(light_space_extent * 0.01f, glm::vec3(0.5f, 0.5f, 1.0f));
    light_space_min -= light_space_padding;
    light_space_max += light_space_padding;

    const float ndc_total_width = (std::max)(light_space_max.x - light_space_min.x, 1.0f);
    const float ndc_total_height = (std::max)(light_space_max.y - light_space_min.y, 1.0f);
    float new_ndc_min_x = ndc_min_x * ndc_total_width + light_space_min.x;
    float new_ndc_min_y = ndc_min_y * ndc_total_height + light_space_min.y;
    float new_ndc_max_x = new_ndc_min_x + ndc_width * ndc_total_width;
    float new_ndc_max_y = new_ndc_min_y + ndc_height * ndc_total_height;

    const float shadowmap_width_safe = static_cast<float>((std::max)(1u, shadowmap_width));
    const float shadowmap_height_safe = static_cast<float>((std::max)(1u, shadowmap_height));
    const float texel_size_x = (new_ndc_max_x - new_ndc_min_x) / shadowmap_width_safe;
    const float texel_size_y = (new_ndc_max_y - new_ndc_min_y) / shadowmap_height_safe;
    if (texel_size_x > 0.0f && texel_size_y > 0.0f)
    {
        const float extent_x = 0.5f * (new_ndc_max_x - new_ndc_min_x);
        const float extent_y = 0.5f * (new_ndc_max_y - new_ndc_min_y);
        float center_x = 0.5f * (new_ndc_min_x + new_ndc_max_x);
        float center_y = 0.5f * (new_ndc_min_y + new_ndc_max_y);
        center_x = std::floor(center_x / texel_size_x) * texel_size_x;
        center_y = std::floor(center_y / texel_size_y) * texel_size_y;
        new_ndc_min_x = center_x - extent_x;
        new_ndc_max_x = center_x + extent_x;
        new_ndc_min_y = center_y - extent_y;
        new_ndc_max_y = center_y + extent_y;
    }

    const float near_plane = light_space_min.z;
    const float far_plane = (std::max)(light_space_max.z, near_plane + 1.0f);
    glm::mat4x4 projection_matrix = glm::orthoLH(
        new_ndc_min_x,
        new_ndc_max_x,
        new_ndc_min_y,
        new_ndc_max_y,
        near_plane,
        far_plane);
    
    out_view_buffer.viewport_width = shadowmap_width;
    out_view_buffer.viewport_height = shadowmap_height;
    out_view_buffer.view_position = {light_eye, 1.0f};
    out_view_buffer.view_projection_matrix = projection_matrix * view_matrix;
    out_view_buffer.prev_view_projection_matrix = out_view_buffer.view_projection_matrix;
    out_view_buffer.inverse_view_projection_matrix = glm::inverse(out_view_buffer.view_projection_matrix);

    out_shadow_info.view_matrix = view_matrix;
    out_shadow_info.projection_matrix = projection_matrix;
    out_shadow_info.shadowmap_size[0] = shadowmap_width;
    out_shadow_info.shadowmap_size[1] = shadowmap_height;
    out_shadow_info.vsm_texture_id = -1;
    
    return true;
}

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

void RendererSystemLighting::LightingPassRuntimeState::Reset()
{
    node = NULL_HANDLE;
    output = NULL_HANDLE;
    shadow_infos_handles.clear();
}

bool RendererSystemLighting::LightingPassRuntimeState::HasInit() const
{
    return node != NULL_HANDLE;
}

void RendererSystemLighting::DirectionalShadowRuntimeState::Reset()
{
    m_resources.clear();
    m_fallback_shadow_maps.clear();
    m_bound_fallback_shadow_map = NULL_HANDLE;
}

bool RendererSystemLighting::DirectionalShadowRuntimeState::HasShadowPasses() const
{
    return !m_resources.empty();
}

size_t RendererSystemLighting::DirectionalShadowRuntimeState::GetShadowPassCount() const
{
    return m_resources.size();
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CreateFallbackShadowMap(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_fallback_shadow_maps.empty())
    {
        return;
    }

    RendererInterface::RenderTargetDesc shadowmap_desc{};
    shadowmap_desc.name = "directional_shadowmap_fallback";
    shadowmap_desc.format = RendererInterface::D32;
    shadowmap_desc.width = 1;
    shadowmap_desc.height = 1;
    shadowmap_desc.clear = RendererInterface::default_clear_depth;
    shadowmap_desc.usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::DEPTH_STENCIL |
        RendererInterface::ResourceUsage::SHADER_RESOURCE);
    m_fallback_shadow_maps =
        resource_operator.CreateFrameBufferedRenderTargets(shadowmap_desc, shadowmap_desc.name);
    if (!m_fallback_shadow_maps.empty())
    {
        m_bound_fallback_shadow_map = m_fallback_shadow_maps.front();
    }
}

RendererSystemLighting::ShadowPassResource& RendererSystemLighting::DirectionalShadowRuntimeState::GetOrCreate(unsigned light_index)
{
    return m_resources[light_index];
}

std::map<unsigned, RendererSystemLighting::ShadowPassResource>& RendererSystemLighting::DirectionalShadowRuntimeState::GetResources()
{
    return m_resources;
}

const std::map<unsigned, RendererSystemLighting::ShadowPassResource>& RendererSystemLighting::DirectionalShadowRuntimeState::GetResources() const
{
    return m_resources;
}

bool RendererSystemLighting::DirectionalShadowRuntimeState::QueueRenderStateUpdates(
    RendererInterface::RenderGraph& graph,
    const RendererInterface::RenderStateDesc& render_state) const
{
    bool queued_all_shadow_passes = HasShadowPasses();
    for (const auto& shadow_resource_pair : m_resources)
    {
        if (!shadow_resource_pair.second.QueueRenderStateUpdate(graph, render_state))
        {
            queued_all_shadow_passes = false;
        }
    }

    return queued_all_shadow_passes;
}

std::vector<RendererInterface::RenderTargetHandle> RendererSystemLighting::DirectionalShadowRuntimeState::SyncAndRegisterShadowPasses(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const std::vector<LightInfo>& lights)
{
    if (!m_fallback_shadow_maps.empty())
    {
        m_bound_fallback_shadow_map = resource_operator.GetFrameBufferedRenderTargetHandle(m_fallback_shadow_maps);
    }

    for (auto& shadow_resource_pair : m_resources)
    {
        auto& shadow_resource = shadow_resource_pair.second;
        shadow_resource.SyncFrameBindings(resource_operator, graph);
        shadow_resource.Register(graph);
    }

    std::vector<RendererInterface::RenderTargetHandle> current_shadow_maps;
    CollectLightIndexedShadowMaps(lights, current_shadow_maps);
    return current_shadow_maps;
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectLightIndexedShadowMaps(
    const std::vector<LightInfo>& lights,
    std::vector<RendererInterface::RenderTargetHandle>& out_shadow_maps) const
{
    out_shadow_maps.clear();
    out_shadow_maps.resize(lights.size(), m_bound_fallback_shadow_map);
    for (unsigned light_index = 0; light_index < lights.size(); ++light_index)
    {
        if (lights[light_index].type != LightType::Directional)
        {
            continue;
        }

        const auto it = m_resources.find(light_index);
        if (it != m_resources.end() && it->second.m_bound_shadow_map != NULL_HANDLE)
        {
            out_shadow_maps[light_index] = it->second.m_bound_shadow_map;
        }
    }
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectLightIndexedShadowMapInfos(
    const std::vector<LightInfo>& lights,
    std::vector<ShadowMapInfo>& out_shadowmap_infos) const
{
    out_shadowmap_infos.clear();
    out_shadowmap_infos.resize(lights.size());
    for (unsigned light_index = 0; light_index < lights.size(); ++light_index)
    {
        if (lights[light_index].type != LightType::Directional)
        {
            continue;
        }

        const auto it = m_resources.find(light_index);
        if (it != m_resources.end())
        {
            out_shadowmap_infos[light_index] = it->second.m_shadow_map_info;
        }
    }
}

void RendererSystemLighting::DirectionalShadowRuntimeState::CollectDependencyNodes(
    std::vector<RendererInterface::RenderGraphNodeHandle>& out_dependency_nodes) const
{
    out_dependency_nodes.reserve(out_dependency_nodes.size() + m_resources.size());
    for (const auto& shadow_resource : m_resources)
    {
        if (shadow_resource.second.m_shadow_pass_node != NULL_HANDLE)
        {
            out_dependency_nodes.push_back(shadow_resource.second.m_shadow_pass_node);
        }
    }
}

bool RendererSystemLighting::ShadowPassResource::HasInit() const
{
    return m_shadow_pass_node != NULL_HANDLE;
}

bool RendererSystemLighting::ShadowPassResource::QueueRenderStateUpdate(
    RendererInterface::RenderGraph& graph,
    const RendererInterface::RenderStateDesc& render_state) const
{
    if (!HasInit())
    {
        return false;
    }

    return graph.QueueNodeRenderStateUpdate(m_shadow_pass_node, render_state);
}

RendererInterface::RenderTargetHandle RendererSystemLighting::ShadowPassResource::SyncFrameBindings(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph)
{
    if (!m_shadow_maps.empty())
    {
        const auto current_shadow_map = resource_operator.GetFrameBufferedRenderTargetHandle(m_shadow_maps);
        if (m_bound_shadow_map == NULL_HANDLE)
        {
            m_bound_shadow_map = current_shadow_map;
        }
        else if (m_bound_shadow_map != current_shadow_map)
        {
            graph.UpdateNodeRenderTargetBinding(m_shadow_pass_node, m_bound_shadow_map, current_shadow_map);
            m_bound_shadow_map = current_shadow_map;
        }
    }

    if (!m_shadow_map_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_shadow_pass_node,
            "ViewBuffer",
            resource_operator.GetFrameBufferedBufferHandle(m_shadow_map_buffer_handles));
    }

    return m_bound_shadow_map;
}

void RendererSystemLighting::ShadowPassResource::Register(RendererInterface::RenderGraph& graph) const
{
    if (HasInit())
    {
        graph.RegisterRenderGraphNode(m_shadow_pass_node);
    }
}

RendererSystemLighting::RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator,
                                               std::shared_ptr<RendererSystemSceneRenderer> scene)
    : m_scene(std::move(scene))
    , m_directional_shadow_render_state(CreateDefaultDirectionalShadowRenderState())
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

const RendererInterface::RenderStateDesc& RendererSystemLighting::GetDirectionalShadowRenderState() const
{
    return m_directional_shadow_render_state;
}

bool RendererSystemLighting::SetDirectionalShadowRenderState(const RendererInterface::RenderStateDesc& render_state)
{
    const bool current_matches = IsEquivalentRenderStateDesc(m_directional_shadow_render_state, render_state);
    const bool pending_matches = m_pending_directional_shadow_render_state.has_value() &&
        IsEquivalentRenderStateDesc(*m_pending_directional_shadow_render_state, render_state);
    if (current_matches && (!m_pending_directional_shadow_render_state.has_value() || pending_matches))
    {
        return false;
    }

    m_directional_shadow_render_state = render_state;
    m_pending_directional_shadow_render_state = render_state;
    return true;
}

bool RendererSystemLighting::SetDirectionalShadowDepthBias(const RendererInterface::DepthBiasDesc& depth_bias)
{
    RendererInterface::RenderStateDesc render_state = m_directional_shadow_render_state;
    render_state.depth_bias = depth_bias;
    return SetDirectionalShadowRenderState(render_state);
}

RendererInterface::RenderTargetHandle RendererSystemLighting::GetLightingOutput() const
{
    return m_lighting_pass_state.output;
}

RendererInterface::RenderStateDesc RendererSystemLighting::CreateDefaultDirectionalShadowRenderState()
{
    RendererInterface::RenderStateDesc render_state{};
    render_state.depth_bias.enabled = true;
    render_state.depth_bias.constant_factor = 256.0f;
    render_state.depth_bias.slope_factor = 2.0f;
    return render_state;
}

bool RendererSystemLighting::Init(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene->HasInit());
    CreateLightingOutput(resource_operator);
    m_directional_shadow_state.CreateFallbackShadowMap(resource_operator);

    auto camera_module = m_scene->GetCameraModule();
    auto width = camera_module->GetWidth();
    auto height = camera_module->GetHeight();

    // Directional light shadow pass
    const auto& lights = m_lighting_module->GetLightInfos();
    for (unsigned i = 0; i < lights.size(); ++i)
    {
        const auto& light = lights[i];
        if (light.type != LightType::Directional)
        {
            continue;
        }

        CreateDirectionalShadowPassResource(resource_operator, graph, i, light);
    }

    CreateLightingPassShadowInfoBuffers(resource_operator);
    m_lighting_pass_state.node =
        graph.CreateRenderGraphNode(resource_operator, BuildLightingPassSetupInfo(camera_module, width, height));

    graph.RegisterRenderTargetToColorOutput(m_lighting_pass_state.output);
    
    return true;
}

bool RendererSystemLighting::HasInit() const
{
    return m_lighting_pass_state.HasInit();
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
    m_directional_shadow_state.Reset();
    m_lighting_pass_state.Reset();
}

bool RendererSystemLighting::Tick(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    const unsigned width = m_scene->GetWidth();
    const unsigned height = m_scene->GetHeight();
    QueuePendingDirectionalShadowRenderStateUpdate(graph);
    graph.UpdateComputeDispatch(m_lighting_pass_state.node, (width + 7) / 8, (height + 7) / 8, 1);

    if (!m_lighting_module->m_light_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "g_lightInfos",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_module->m_light_buffer_handles));
    }
    if (!m_lighting_module->m_light_count_buffer_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "LightInfoConstantBuffer",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_module->m_light_count_buffer_handles));
    }
    if (!m_lighting_pass_state.shadow_infos_handles.empty())
    {
        graph.UpdateNodeBufferBinding(
            m_lighting_pass_state.node,
            "g_shadowmap_infos",
            resource_operator.GetFrameBufferedBufferHandle(m_lighting_pass_state.shadow_infos_handles));
    }

    if (CastShadow())
    {
        UpdateDirectionalShadowResources(resource_operator);
        auto current_shadow_maps = m_directional_shadow_state.SyncAndRegisterShadowPasses(
            resource_operator,
            graph,
            m_lighting_module->GetLightInfos());

        if (!current_shadow_maps.empty())
        {
            graph.UpdateNodeRenderTargetTextureBinding(
                m_lighting_pass_state.node,
                "bindless_shadowmap_textures",
                current_shadow_maps);
        }
    }
    
    graph.RegisterRenderGraphNode(m_lighting_pass_state.node);
    
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
    ImGui::Text("Directional Shadow Maps: %u", static_cast<unsigned>(m_directional_shadow_state.GetShadowPassCount()));
}

bool RendererSystemLighting::QueuePendingDirectionalShadowRenderStateUpdate(RendererInterface::RenderGraph& graph)
{
    if (!m_pending_directional_shadow_render_state.has_value())
    {
        return true;
    }

    const bool queued_all_shadow_passes =
        m_directional_shadow_state.QueueRenderStateUpdates(graph, *m_pending_directional_shadow_render_state);

    if (queued_all_shadow_passes)
    {
        m_pending_directional_shadow_render_state.reset();
    }

    return queued_all_shadow_passes;
}

void RendererSystemLighting::CreateLightingOutput(RendererInterface::ResourceOperator& resource_operator)
{
    m_lighting_pass_state.output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "LightingPass_Output",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(
            RendererInterface::ResourceUsage::RENDER_TARGET |
            RendererInterface::ResourceUsage::COPY_SRC |
            RendererInterface::ResourceUsage::UNORDER_ACCESS |
            RendererInterface::ResourceUsage::SHADER_RESOURCE));
}

void RendererSystemLighting::CreateLightingPassShadowInfoBuffers(RendererInterface::ResourceOperator& resource_operator)
{
    std::vector<ShadowMapInfo> shadowmap_infos;
    m_directional_shadow_state.CollectLightIndexedShadowMapInfos(m_lighting_module->GetLightInfos(), shadowmap_infos);

    RendererInterface::BufferDesc shadowmap_info_buffer_desc{};
    shadowmap_info_buffer_desc.type = RendererInterface::DEFAULT;
    shadowmap_info_buffer_desc.name = "g_shadowmap_infos";
    shadowmap_info_buffer_desc.size = sizeof(ShadowMapInfo) * shadowmap_infos.size();
    shadowmap_info_buffer_desc.usage = RendererInterface::USAGE_SRV;
    shadowmap_info_buffer_desc.data = shadowmap_infos.data();
    m_lighting_pass_state.shadow_infos_handles =
        resource_operator.CreateFrameBufferedBuffers(shadowmap_info_buffer_desc, "g_shadowmap_infos");
}

RendererSystemLighting::ShadowPassResource& RendererSystemLighting::CreateDirectionalShadowPassResource(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    unsigned light_index,
    const LightInfo& light_info)
{
    auto& shadow_pass_resource = m_directional_shadow_state.GetOrCreate(light_index);
    const std::string shadowmap_name = std::format("directional_shadowmap_{}", light_index);

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
    shadow_pass_resource.m_shadow_maps =
        resource_operator.CreateFrameBufferedRenderTargets(shadowmap_desc, shadowmap_name);
    GLTF_CHECK(!shadow_pass_resource.m_shadow_maps.empty());
    shadow_pass_resource.m_bound_shadow_map = shadow_pass_resource.m_shadow_maps[0];

    ShadowPassResource::CalcDirectionalLightShadowMatrix(
        light_info,
        m_scene->GetSceneMeshModule()->GetSceneBounds(),
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        shadowmap_width,
        shadowmap_height,
        shadow_pass_resource.m_shadow_map_view_buffer,
        shadow_pass_resource.m_shadow_map_info);
    RendererInterface::BufferDesc camera_buffer_desc{};
    camera_buffer_desc.name = "ViewBuffer";
    camera_buffer_desc.size = sizeof(ViewBuffer);
    camera_buffer_desc.type = RendererInterface::DEFAULT;
    camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
    camera_buffer_desc.data = &shadow_pass_resource.m_shadow_map_view_buffer;
    shadow_pass_resource.m_shadow_map_buffer_handles =
        resource_operator.CreateFrameBufferedBuffers(
            camera_buffer_desc,
            std::format("ViewBuffer_shadow_{}", light_index));
    shadow_pass_resource.m_shadow_map_view_buffers.assign(
        shadow_pass_resource.m_shadow_map_buffer_handles.size(),
        shadow_pass_resource.m_shadow_map_view_buffer);

    shadow_pass_resource.m_shadow_pass_node = graph.CreateRenderGraphNode(
        resource_operator,
        BuildDirectionalShadowPassSetupInfo(shadow_pass_resource, light_index));
    GLTF_CHECK(!shadow_pass_resource.m_shadow_map_buffer_handles.empty());
    return shadow_pass_resource;
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemLighting::BuildDirectionalShadowPassSetupInfo(
    const ShadowPassResource& shadow_pass_resource,
    unsigned light_index) const
{
    RendererInterface::RenderGraph::RenderPassSetupInfo setup_info{};
    setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
    setup_info.debug_group = "Lighting";
    setup_info.debug_name = std::format("Directional Shadow {}", light_index);
    setup_info.modules = {m_scene->GetSceneMeshModule()};
    setup_info.render_state = m_directional_shadow_render_state;
    setup_info.excluded_buffer_bindings.insert("g_material_infos");
    setup_info.excluded_texture_bindings.insert("bindless_material_textures");
    setup_info.shader_setup_infos = {
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
    setup_info.render_targets = {
        {shadow_pass_resource.m_bound_shadow_map, depth_binding_desc}
    };

    RendererInterface::BufferBindingDesc camera_binding_desc{};
    camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    camera_binding_desc.buffer_handle = shadow_pass_resource.m_shadow_map_buffer_handles[0];
    camera_binding_desc.is_structured_buffer = false;
    setup_info.buffer_resources["ViewBuffer"] = camera_binding_desc;
    setup_info.viewport_width = shadow_pass_resource.m_shadow_map_info.shadowmap_size[0];
    setup_info.viewport_height = shadow_pass_resource.m_shadow_map_info.shadowmap_size[1];
    return setup_info;
}

RendererInterface::RenderGraph::RenderPassSetupInfo RendererSystemLighting::BuildLightingPassSetupInfo(
    const std::shared_ptr<RendererModuleCamera>& camera_module,
    unsigned width,
    unsigned height) const
{
    RendererInterface::RenderGraph::RenderPassSetupInfo setup_info{};
    setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    setup_info.debug_group = "Lighting";
    setup_info.debug_name = "Scene Lighting";
    setup_info.modules = {m_lighting_module, camera_module};
    setup_info.shader_setup_infos = {
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
    output_binding_desc.render_target_texture = {m_lighting_pass_state.output};

    RendererInterface::RenderTargetTextureBindingDesc shadowmap_binding_desc{};
    shadowmap_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    shadowmap_binding_desc.name = "bindless_shadowmap_textures";
    std::vector<RendererInterface::RenderTargetHandle> initial_shadow_maps;
    m_directional_shadow_state.CollectLightIndexedShadowMaps(m_lighting_module->GetLightInfos(), initial_shadow_maps);
    for (const auto& shadow_map : initial_shadow_maps)
    {
        shadowmap_binding_desc.render_target_texture.push_back(shadow_map);
    }
    setup_info.sampled_render_targets = {
        albedo_binding_desc,
        normal_binding_desc,
        depth_binding_desc,
        shadowmap_binding_desc,
        output_binding_desc
    };

    RendererInterface::BufferBindingDesc shadowmap_info_binding_desc{};
    shadowmap_info_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    GLTF_CHECK(!m_lighting_pass_state.shadow_infos_handles.empty());
    shadowmap_info_binding_desc.buffer_handle = m_lighting_pass_state.shadow_infos_handles.front();
    shadowmap_info_binding_desc.is_structured_buffer = true;
    shadowmap_info_binding_desc.stride = sizeof(ShadowMapInfo);
    shadowmap_info_binding_desc.count = m_lighting_module->GetLightInfos().size();
    setup_info.buffer_resources["g_shadowmap_infos"] = shadowmap_info_binding_desc;

    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = (width + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = (height + 7) / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    setup_info.execute_command = dispatch_command;
    m_directional_shadow_state.CollectDependencyNodes(setup_info.dependency_render_graph_nodes);
    return setup_info;
}

void RendererSystemLighting::UpdateDirectionalShadowResources(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_directional_shadow_state.HasShadowPasses())
    {
        return;
    }

    std::vector<ShadowMapInfo> shadowmap_infos;
    const unsigned current_frame_slot_index = resource_operator.GetCurrentFrameSlotIndex();

    const auto& lights = m_lighting_module->GetLightInfos();
    const auto scene_bounds = m_scene->GetSceneMeshModule()->GetSceneBounds();
    for (auto& shadow_resource_pair : m_directional_shadow_state.GetResources())
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
    }
    m_directional_shadow_state.CollectLightIndexedShadowMapInfos(lights, shadowmap_infos);

    if (!shadowmap_infos.empty() && !m_lighting_pass_state.shadow_infos_handles.empty())
    {
        RendererInterface::BufferUploadDesc shadow_info_upload_desc{};
        shadow_info_upload_desc.data = shadowmap_infos.data();
        shadow_info_upload_desc.size = shadowmap_infos.size() * sizeof(ShadowMapInfo);
        resource_operator.UploadFrameBufferedBufferData(m_lighting_pass_state.shadow_infos_handles, shadow_info_upload_desc);
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

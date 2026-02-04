// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "RendererSystemLighting.h"
#include <format>

#include "RendererSceneAABB.h"

RendererSystemLighting::RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator,
                                               std::shared_ptr<RendererSystemSceneRenderer> scene)
    : m_scene(std::move(scene))
{
    m_lighting_module = std::make_shared<RendererModuleLighting>(resource_operator);
    m_modules.push_back(m_lighting_module);
}

bool RendererSystemLighting::AddLight(const LightInfo& light_info)
{
    GLTF_CHECK(m_lighting_module);
    m_lighting_module->AddLightInfo(light_info);
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

bool RendererSystemLighting::Init(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene->HasInit());
    
    auto lighting_pass_output = resource_operator.CreateRenderTarget("LightingPass_Output", m_scene->GetWidth(), m_scene->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::UNORDER_ACCESS));
    
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
        const std::string shadowmap_name = std::format("directional_shadowmap_%d", i);

        const unsigned shadowmap_width = 1024;
        const unsigned shadowmap_height = 1024;
        new_shadow_pass_resource.m_shadow_map = resource_operator.CreateRenderTarget(
            shadowmap_name, shadowmap_width, shadowmap_height, RendererInterface::D32,
            RendererInterface::default_clear_depth,
            static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL | RendererInterface::ResourceUsage::SHADER_RESOURCE) );

        ShadowPassResource::CalcDirectionalLightShadowMatrix(light, m_scene->GetSceneMeshModule()->GetSceneBounds(),
            0.0f, 0.0f, 1.0f, 1.0f, shadowmap_width, shadowmap_height, new_shadow_pass_resource.m_shadow_map_view_buffer, new_shadow_pass_resource.m_shadow_map_info);
        
        RendererInterface::BufferDesc camera_buffer_desc{};
        camera_buffer_desc.name = "ViewBuffer";
        camera_buffer_desc.size = sizeof(ViewBuffer);
        camera_buffer_desc.type = RendererInterface::UPLOAD;
        camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
        camera_buffer_desc.data = &new_shadow_pass_resource.m_shadow_map_view_buffer;
        new_shadow_pass_resource.m_shadow_map_buffer_handle = resource_operator.CreateBuffer(camera_buffer_desc);

        RendererInterface::RenderGraph::RenderPassSetupInfo shadow_pass_setup_info{};
        shadow_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
        shadow_pass_setup_info.modules = {m_scene->GetSceneMeshModule()};
        shadow_pass_setup_info.shader_setup_infos = {
            {
                .shader_type = RendererInterface::ShaderType::VERTEX_SHADER,
                .entry_function = "MainVS",
                .shader_file = "Resources/Shaders/ModelRenderingShader.hlsl"
            },
            {
                .shader_type = RendererInterface::ShaderType::FRAGMENT_SHADER,
                .entry_function = "MainFS",
                .shader_file = "Resources/Shaders/ModelRenderingShader.hlsl"
            }
        };

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;

        shadow_pass_setup_info.render_targets = {
            {new_shadow_pass_resource.m_shadow_map, depth_binding_desc}
        };
        
        RendererInterface::BufferBindingDesc camera_binding_desc{};
        camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
        camera_binding_desc.buffer_handle = new_shadow_pass_resource.m_shadow_map_buffer_handle;
        camera_binding_desc.is_structured_buffer = false;
        shadow_pass_setup_info.buffer_resources[camera_buffer_desc.name] = camera_binding_desc;

        shadow_pass_setup_info.viewport_width = shadowmap_width;
        shadow_pass_setup_info.viewport_height = shadowmap_height;
        new_shadow_pass_resource.m_shadow_pass_node = graph.CreateRenderGraphNode(resource_operator, shadow_pass_setup_info);
    }
    
    RendererInterface::RenderGraph::RenderPassSetupInfo lighting_pass_setup_info{};
    lighting_pass_setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
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
    output_binding_desc.render_target_texture = {lighting_pass_output};

    RendererInterface::RenderTargetTextureBindingDesc shadowmap_binding_desc{};
    shadowmap_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    shadowmap_binding_desc.name = "bindless_shadowmap_textures";
    for (const auto& shadow_resource : m_shadow_pass_resources)
    {
        shadowmap_binding_desc.render_target_texture.push_back(shadow_resource.second.m_shadow_map);    
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
    m_lighting_pass_shadow_infos_handle = resource_operator.CreateBuffer(shadowmap_info_buffer_desc);

    RendererInterface::BufferBindingDesc shadowmap_info_binding_desc{};
    shadowmap_info_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    shadowmap_info_binding_desc.buffer_handle = m_lighting_pass_shadow_infos_handle;
    shadowmap_info_binding_desc.is_structured_buffer = true;
    shadowmap_info_binding_desc.stride = sizeof(ShadowMapInfo);
    shadowmap_info_binding_desc.count = shadowmap_infos.size();
    lighting_pass_setup_info.buffer_resources["g_shadowmap_infos"] = shadowmap_info_binding_desc;
    
    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = width / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = height / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    lighting_pass_setup_info.execute_command = dispatch_command;

    m_lighting_pass_node = graph.CreateRenderGraphNode(resource_operator, lighting_pass_setup_info);

    graph.RegisterRenderTargetToColorOutput(lighting_pass_output);
    
    return true;
}

bool RendererSystemLighting::HasInit() const
{
    return m_lighting_pass_node != NULL_HANDLE;
}

bool RendererSystemLighting::Tick(RendererInterface::ResourceOperator& resource_operator,
                                  RendererInterface::RenderGraph& graph, unsigned long long interval)
{
    if (CastShadow())
    {
        for (const auto& shadow_pass: m_shadow_pass_resources)
        {
            graph.RegisterRenderGraphNode(shadow_pass.second.m_shadow_pass_node);
        }
    }
    
    graph.RegisterRenderGraphNode(m_lighting_pass_node);
    
    m_lighting_module->Tick(resource_operator, interval);
    
    return true;
}

bool RendererSystemLighting::ShadowPassResource::CalcDirectionalLightShadowMatrix(
    const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y,
    float ndc_width, float ndc_height, unsigned shadowmap_width, unsigned shadowmap_height,
    ViewBuffer& out_view_buffer, ShadowMapInfo& out_shadow_info)
{
    const auto& scene_bounds_min = scene_bounds.getMin();
    const auto& scene_bounds_max = scene_bounds.getMax();
    const auto& scene_center = scene_bounds.getCenter();
    
    glm::mat4x4 view_matrix = glm::lookAtLH(scene_center, scene_center + directional_light_info.position , {0.0f, 1.0f, 0.0f});
    glm::vec3 light_ndc_min{ FLT_MAX, FLT_MAX, FLT_MAX };
    glm::vec3 light_ndc_max{ FLT_MIN, FLT_MIN, FLT_MIN };

    glm::vec4 scene_corner[8] =
        {
        {scene_bounds_min.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_min.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_max.y, scene_bounds_min.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_min.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_min.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f},
        {scene_bounds_max.x, scene_bounds_max.y, scene_bounds_max.z, 1.0f},
    };

    for (unsigned i = 0; i < 8; ++i)
    {
        glm::vec4 ndc_to_light_view = view_matrix * scene_corner[i];

        light_ndc_min.x = (light_ndc_min.x > ndc_to_light_view.x) ? ndc_to_light_view.x : light_ndc_min.x;
        light_ndc_min.y = (light_ndc_min.y > ndc_to_light_view.y) ? ndc_to_light_view.y : light_ndc_min.y;
        light_ndc_min.z = (light_ndc_min.z > ndc_to_light_view.z) ? ndc_to_light_view.z : light_ndc_min.z;

        light_ndc_max.x = (light_ndc_max.x < ndc_to_light_view.x) ? ndc_to_light_view.x : light_ndc_max.x;
        light_ndc_max.y = (light_ndc_max.y < ndc_to_light_view.y) ? ndc_to_light_view.y : light_ndc_max.y;
        light_ndc_max.z = (light_ndc_max.z < ndc_to_light_view.z) ? ndc_to_light_view.z : light_ndc_max.z;
    }

    // for safety
    auto ndc_size = light_ndc_max - light_ndc_min;
    float safety_factor = 0.01f;
    light_ndc_min -= ndc_size * safety_factor;
    light_ndc_max += ndc_size * safety_factor;
    
    // Apply ndc range
    float ndc_total_width = light_ndc_max.x - light_ndc_min.x;
    float ndc_total_height = light_ndc_max.y - light_ndc_min.y;

    float new_ndc_min_x = ndc_min_x * ndc_total_width + light_ndc_min.x;
    float new_ndc_min_y = ndc_min_y * ndc_total_height + light_ndc_min.y;
    float new_ndc_max_x = new_ndc_min_x + ndc_width * ndc_total_width;
    float new_ndc_max_y = new_ndc_min_y + ndc_height * ndc_total_height;
        
    glm::mat4x4 projection_matrix = glm::orthoLH(new_ndc_min_x, new_ndc_max_x, new_ndc_min_y, new_ndc_max_y, light_ndc_min.z, light_ndc_max.z);
    
    out_view_buffer.viewport_width = shadowmap_width;
    out_view_buffer.viewport_height = shadowmap_height;
    out_view_buffer.view_position = {scene_center, 1.0f};
    out_view_buffer.view_projection_matrix = projection_matrix * view_matrix;
    out_view_buffer.inverse_view_projection_matrix = glm::inverse(out_view_buffer.view_projection_matrix);

    out_shadow_info.view_matrix = view_matrix;
    out_shadow_info.projection_matrix = projection_matrix;
    out_shadow_info.shadowmap_size[0] = shadowmap_width;
    out_shadow_info.shadowmap_size[1] = shadowmap_height;
    out_shadow_info.vsm_texture_id = -1;
    
    return true;
}

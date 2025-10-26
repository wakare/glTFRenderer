#include "RendererSystemLighting.h"

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

bool RendererSystemLighting::Init(RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene->HasInit());
    
    auto lighting_pass_output = resource_operator.CreateRenderTarget("LightingPass_Output", m_scene->GetWidth(), m_scene->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::UNORDER_ACCESS));
    
    auto m_camera_module = m_scene->GetCameraModule();
    auto width = m_camera_module->GetWidth();
    auto height = m_camera_module->GetHeight();
    
    RendererInterface::RenderGraph::RenderPassSetupInfo setup_info{};
    setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
    setup_info.modules = {m_lighting_module, m_camera_module};
    setup_info.shader_setup_infos = {
        {RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/SceneLighting.hlsl"}
    };
    RendererSystemOutput<RendererSystemSceneRenderer> output;
    
    RendererInterface::RenderTargetTextureBindingDesc albedo_binding_desc{};
    albedo_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    albedo_binding_desc.name = "albedoTex";
    albedo_binding_desc.render_target_texture = output.GetRenderTargetHandle(*m_scene, "m_base_pass_color");
        
    RendererInterface::RenderTargetTextureBindingDesc normal_binding_desc{};
    normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    normal_binding_desc.name = "normalTex";
    normal_binding_desc.render_target_texture = output.GetRenderTargetHandle(*m_scene, "m_base_pass_normal");
        
    RendererInterface::RenderTargetTextureBindingDesc depth_binding_desc{};
    depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
    depth_binding_desc.name = "depthTex";
    depth_binding_desc.render_target_texture = output.GetRenderTargetHandle(*m_scene, "m_base_pass_depth");

    RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
    output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
    output_binding_desc.name = "Output";
    output_binding_desc.render_target_texture = lighting_pass_output;
    setup_info.sampled_render_targets = {
        {albedo_binding_desc.render_target_texture, albedo_binding_desc},
        {normal_binding_desc.render_target_texture, normal_binding_desc},
        {depth_binding_desc.render_target_texture, depth_binding_desc},
        {lighting_pass_output, output_binding_desc}
    };
    RendererInterface::RenderExecuteCommand dispatch_command{};
    dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
    dispatch_command.parameter.dispatch_parameter.group_size_x = width / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_y = height / 8;
    dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
    setup_info.execute_command = dispatch_command;

    m_lighting_pass_node = graph.CreateRenderGraphNode(resource_operator, setup_info);

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
    // add pass node
    graph.RegisterRenderGraphNode(m_lighting_pass_node);
    
    m_lighting_module->Tick(resource_operator, interval);
    
    return true;
}

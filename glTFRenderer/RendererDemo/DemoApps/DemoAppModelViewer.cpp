#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include "SceneRendererUtil/SceneRendererMeshDrawDispatcher.h"
#include <glm/glm/gtx/norm.hpp>
#include <imgui/imgui.h>

#include "RendererCommon.h"

void DemoAppModelViewer::TickFrame(unsigned long long delta_time_ms)
{
    auto& input_device = m_window->GetInputDevice();
    if (m_camera_operator)
    {
        m_camera_operator->TickCameraOperation(input_device, *m_resource_manager, delta_time_ms);    
    }

    input_device.TickFrame(delta_time_ms);
}

void DemoAppModelViewer::Run(const std::vector<std::string>& arguments)
{
    InitRenderContext(arguments);

    RendererCameraDesc camera_desc{};
    camera_desc.mode = CameraMode::Free;
    camera_desc.transform = glm::mat4(1.0f);
    camera_desc.fov_angle = 90.0f;
    camera_desc.projection_far = 1000.0f;
    camera_desc.projection_near = 0.001f;
    camera_desc.projection_width = static_cast<float>(m_width);
    camera_desc.projection_height = static_cast<float>(m_height);
    m_camera_operator = std::make_unique<SceneRendererCameraOperator>(*m_resource_manager, camera_desc);
    m_draw_dispatcher = std::make_unique<SceneRendererMeshDrawDispatcher>(*m_resource_manager, "glTFResources/Models/Sponza/glTF/Sponza.gltf");

    // Create render target resource
    auto base_pass_albedo_output = CreateRenderTarget("BasePass_Color", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
    static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    auto base_pass_normal_output = CreateRenderTarget("BasePass_Normal", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
    static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    auto depth_handle = CreateRenderTarget("Depth", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::D32, RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL | RendererInterface::ResourceUsage::SHADER_RESOURCE));

    auto lighting_pass_output = CreateRenderTarget("LightingPass_Output", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC | RendererInterface::ResourceUsage::UNORDER_ACCESS));
    
    // Setup base pass rendering config
    {
        // Create shader resource
        auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainVS");
        auto fragment_shader_handle = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainFS");

        RendererInterface::RenderPassDesc render_pass_desc{};
        render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
        render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);

        RendererInterface::RenderTargetBindingDesc color_rt_binding_desc{};
        color_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        color_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;

        RendererInterface::RenderTargetBindingDesc normal_rt_binding_desc{};
        normal_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        normal_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;

        render_pass_desc.render_target_bindings.push_back(color_rt_binding_desc);
        render_pass_desc.render_target_bindings.push_back(normal_rt_binding_desc);
    
        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        render_pass_desc.render_target_bindings.push_back(depth_binding_desc);
    
        render_pass_desc.type = RendererInterface::RenderPassType::GRAPHICS;

        auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
    
        RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
        render_pass_draw_desc.render_target_resources.emplace(base_pass_albedo_output,
            RendererInterface::RenderTargetBindingDesc
            {
                .format = RendererInterface::RGBA8_UNORM,
                .usage = RendererInterface::RenderPassResourceUsage::COLOR,
            });
        render_pass_draw_desc.render_target_clear_states.emplace(base_pass_albedo_output, true);

        render_pass_draw_desc.render_target_resources.emplace(base_pass_normal_output,
                RendererInterface::RenderTargetBindingDesc
                {
                    .format = RendererInterface::RGBA8_UNORM,
                    .usage = RendererInterface::RenderPassResourceUsage::COLOR,
                });
        render_pass_draw_desc.render_target_clear_states.emplace(base_pass_normal_output, true);
        
        render_pass_draw_desc.render_target_resources.emplace(depth_handle,
        RendererInterface::RenderTargetBindingDesc
        {
            .format = RendererInterface::D32,
            .usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL,
        });
        render_pass_draw_desc.render_target_clear_states.emplace(depth_handle, true);
    
        m_draw_dispatcher->BindDrawCommands(render_pass_draw_desc);
        m_camera_operator->BindDrawCommands(render_pass_draw_desc);

        RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
        render_graph_node_desc.draw_info = render_pass_draw_desc;
        render_graph_node_desc.render_pass_handle = render_pass_handle;
        render_graph_node_desc.pre_render_callback = [this](unsigned long long interval){TickFrame(interval);};

        auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
        m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);
    }
    
    // Setup lighting pass config
    {
        m_lighting_module = std::make_unique<RendererModuleLighting>(*m_resource_manager);
        
        // Add test light
        LightInfo directional_light_info{};
        directional_light_info.type = Directional;
        directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
        directional_light_info.position = {0.0f, -0.707f, -0.707f};
        directional_light_info.radius = 100000.0f;
        m_lighting_module->AddLightInfo(directional_light_info);
        m_lighting_module->FinalizeModule(*m_resource_manager);
        
        auto lighting_compute_shader = CreateShader(RendererInterface::COMPUTE_SHADER, "Resources/Shaders/SceneLighting.hlsl", "main");
        RendererInterface::RenderPassDesc render_pass_desc{};
        render_pass_desc.shaders.emplace(RendererInterface::ShaderType::COMPUTE_SHADER, lighting_compute_shader);
        render_pass_desc.type = RendererInterface::RenderPassType::COMPUTE;

        auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
        RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
        RendererInterface::RenderTargetTextureBindingDesc albedo_binding_desc{};
        albedo_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        albedo_binding_desc.render_target_texture = base_pass_albedo_output;
        render_pass_draw_desc.render_target_texture_resources["albedoTex"] = albedo_binding_desc;
        
        RendererInterface::RenderTargetTextureBindingDesc normal_binding_desc{};
        normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        normal_binding_desc.render_target_texture = base_pass_normal_output;
        render_pass_draw_desc.render_target_texture_resources["normalTex"] = normal_binding_desc;
        
        RendererInterface::RenderTargetTextureBindingDesc depth_binding_desc{};
        depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        depth_binding_desc.render_target_texture = depth_handle;
        render_pass_draw_desc.render_target_texture_resources["depthTex"] = depth_binding_desc;

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.render_target_texture=lighting_pass_output;
        render_pass_draw_desc.render_target_texture_resources["Output"] = output_binding_desc;
        
        m_lighting_module->BindDrawCommands(render_pass_draw_desc);
        m_camera_operator->BindDrawCommands(render_pass_draw_desc);
        
        RendererInterface::RenderExecuteCommand dispatch_command{};
        dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
        dispatch_command.parameter.dispatch_parameter.group_size_x = m_window->GetWidth() / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_y = m_window->GetHeight() / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
        render_pass_draw_desc.execute_commands.push_back(dispatch_command);
        
        RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
        render_graph_node_desc.draw_info = render_pass_draw_desc;
        render_graph_node_desc.render_pass_handle = render_pass_handle;

        auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
        m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);
    }
    m_render_graph->RegisterRenderTargetToColorOutput(lighting_pass_output);
    
    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    m_window->EnterWindowEventLoop();
}
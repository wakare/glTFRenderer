#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include <glm/glm/gtx/norm.hpp>
#include <imgui/imgui.h>

#include "RendererCommon.h"
#include "RendererModule/RendererModuleCamera.h"
#include "RendererModule/RendererModuleSceneMesh.h"

void DemoAppModelViewer::TickFrameInternal(unsigned long long time_interval)
{
    auto& input_device = m_window->GetInputDevice();
    if (m_camera_module)
    {
        m_camera_module->TickCameraOperation(input_device, time_interval);    
    }

    input_device.TickFrame(time_interval);
}

bool DemoAppModelViewer::InitInternal(const std::vector<std::string>& arguments)
{
    RendererCameraDesc camera_desc{};
    camera_desc.mode = CameraMode::Free;
    camera_desc.transform = glm::mat4(1.0f);
    camera_desc.fov_angle = 90.0f;
    camera_desc.projection_far = 1000.0f;
    camera_desc.projection_near = 0.001f;
    camera_desc.projection_width = static_cast<float>(m_width);
    camera_desc.projection_height = static_cast<float>(m_height);
    
    m_camera_module = std::make_shared<RendererModuleCamera>(*m_resource_manager, camera_desc);
    m_scene_mesh_module = std::make_shared<RendererModuleSceneMesh>(*m_resource_manager, "glTFResources/Models/Sponza/glTF/Sponza.gltf");
    m_lighting_module = std::make_shared<RendererModuleLighting>(*m_resource_manager);

    m_modules.push_back(m_camera_module);
    m_modules.push_back(m_lighting_module);
    m_modules.push_back(m_scene_mesh_module);
        
    // Add test light
    LightInfo directional_light_info{};
    directional_light_info.type = Directional;
    directional_light_info.intensity = {1.0f, 1.0f, 1.0f};
    directional_light_info.position = {0.0f, -0.707f, -0.707f};
    directional_light_info.radius = 100000.0f;
    m_lighting_module->AddLightInfo(directional_light_info);
        
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
        RenderPassSetupInfo setup_info{};
        setup_info.render_pass_type = RendererInterface::RenderPassType::GRAPHICS;
        setup_info.modules = {m_scene_mesh_module, m_camera_module};
        setup_info.shader_setup_infos = {
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

        RendererInterface::RenderTargetBindingDesc color_rt_binding_desc{};
        color_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        color_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        color_rt_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc normal_rt_binding_desc{};
        normal_rt_binding_desc.format = RendererInterface::RGBA8_UNORM;
        normal_rt_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        normal_rt_binding_desc.need_clear = true;

        RendererInterface::RenderTargetBindingDesc depth_binding_desc{};
        depth_binding_desc.format = RendererInterface::D32;
        depth_binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        depth_binding_desc.need_clear = true;
        
        setup_info.render_targets = {
            {base_pass_albedo_output, color_rt_binding_desc},
            {base_pass_normal_output, normal_rt_binding_desc},
            {depth_handle, depth_binding_desc}
        };
        SetupPassNode(setup_info);
    }
    
    // Setup lighting pass config
    {
        RenderPassSetupInfo setup_info{};
        setup_info.render_pass_type = RendererInterface::RenderPassType::COMPUTE;
        setup_info.modules = {m_lighting_module, m_camera_module};
        setup_info.shader_setup_infos = {
            {RendererInterface::COMPUTE_SHADER, "main", "Resources/Shaders/SceneLighting.hlsl"}
        };

        RendererInterface::RenderTargetTextureBindingDesc albedo_binding_desc{};
        albedo_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        albedo_binding_desc.name = "albedoTex";
        albedo_binding_desc.render_target_texture = base_pass_albedo_output;
        
        RendererInterface::RenderTargetTextureBindingDesc normal_binding_desc{};
        normal_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        normal_binding_desc.name = "normalTex";
        normal_binding_desc.render_target_texture = base_pass_normal_output;
        
        RendererInterface::RenderTargetTextureBindingDesc depth_binding_desc{};
        depth_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::SRV;
        depth_binding_desc.name = "depthTex";
        depth_binding_desc.render_target_texture = depth_handle;

        RendererInterface::RenderTargetTextureBindingDesc output_binding_desc{};
        output_binding_desc.type = RendererInterface::RenderTargetTextureBindingDesc::UAV;
        output_binding_desc.name = "Output";
        output_binding_desc.render_target_texture = lighting_pass_output;
        setup_info.sampled_render_targets = {
            {base_pass_albedo_output, albedo_binding_desc},
            {base_pass_normal_output, normal_binding_desc},
            {depth_handle, depth_binding_desc},
            {lighting_pass_output, output_binding_desc}
        };
        RendererInterface::RenderExecuteCommand dispatch_command{};
        dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
        dispatch_command.parameter.dispatch_parameter.group_size_x = m_window->GetWidth() / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_y = m_window->GetHeight() / 8;
        dispatch_command.parameter.dispatch_parameter.group_size_z = 1;
        setup_info.execute_command = dispatch_command;

        SetupPassNode(setup_info);
    }
    
    m_render_graph->RegisterRenderTargetToColorOutput(lighting_pass_output);
    
    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();

    return true;
}
#include "DemoAppModelViewer.h"

#include "RenderWindow/RendererInputDevice.h"
#include "SceneRendererUtil/SceneRendererDrawDispatcher.h"
#include <glm/glm/gtx/norm.hpp>

#include "RendererCommon.h"

void DemoAppModelViewer::TickFrame(unsigned long long delta_time_ms)
{
    auto& input_device = m_window->GetInputDevice();
    bool need_apply_movement = false;
    glm::fvec3 delta_translation = {0.0f, 0.0f, 0.0f};
    glm::fvec3 delta_rotation = {0.0f, 0.0f, 0.0f};
    
    if (m_camera->GetCameraMode() == CameraMode::Free)
    {
        // Handle movement
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_W))
        {
            delta_translation.z += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_S))
        {
            delta_translation.z -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_A))
        {
            delta_translation.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_D))
        {
            delta_translation.x += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_Q))
        {
            delta_translation.y += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_E))
        {
            delta_translation.y -= 1.0f;
            need_apply_movement = true;
        }
    }
    else
    {
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_W))
        {
            delta_rotation.x += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_S))
        {
            delta_rotation.x -= 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_A))
        {
            delta_rotation.y += 1.0f;
            need_apply_movement = true;
        }
    
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_D))
        {
            delta_rotation.y -= 1.0f;
            need_apply_movement = true;
        }

        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_Q))
        {
            delta_translation.z += (m_camera->GetObserveDistance() + 1.0f);
            need_apply_movement = true;
        }
        
        if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_E))
        {
            delta_translation.z -= (m_camera->GetObserveDistance() + 1.0f);
            need_apply_movement = true;
        }
    }
    
    if (input_device.IsKeyPressed(InputDeviceKeyType::KEY_LEFT_CONTROL) ||
        input_device.IsMouseButtonPressed(InputDeviceButtonType::MOUSE_BUTTON_LEFT))
    {
        const auto cursor_offset = input_device.GetCursorOffset();
        
        delta_rotation.y -= cursor_offset.X;
        delta_rotation.x -= cursor_offset.Y;
        if (fabs(delta_rotation.x) > 0.0f || fabs(delta_rotation.y) > 0.0f)
        {
            need_apply_movement = true;
            //LOG_FORMAT_FLUSH("[DEBUG] CURSOR OFFSET %f %f\n",cursor_offset.X, cursor_offset.Y );
        }
    }

    if (!need_apply_movement)
    {
        return;
    }
    
    // Apply movement to scene view
    const float translation_scale = static_cast<float>(delta_time_ms) / 1000.0f;
    const float rotation_scale = static_cast<float>(delta_time_ms) / 1000.0f;
    
    // Convert view space translation to world space
    delta_translation = glm::inverse(m_camera->GetViewMatrix()) * glm::fvec4{delta_translation, 0.0f};
    delta_translation *= translation_scale;
    
    delta_rotation *= rotation_scale;

    if (m_camera->GetCameraMode() == CameraMode::Free)
    {
        if (glm::length2(delta_translation) > 0)
        {
            m_camera->TranslateOffset(delta_translation);    
        }

        if (glm::length2(delta_rotation) > 0)
        {
            m_camera->RotateEulerAngleOffset(delta_rotation);    
        }
    }
    else if (m_camera->GetCameraMode() ==  CameraMode::Observer)
    {
        m_camera->TranslateOffset(delta_translation);
        m_camera->ObserveRotateXY(delta_rotation.x, delta_rotation.y);
    }
    
    m_camera->MarkTransformDirty();

    auto camera_transform = m_camera->GetViewProjectionMatrix();
    RendererInterface::BufferUploadDesc camera_buffer_upload_desc{};
    camera_buffer_upload_desc.data = &camera_transform;
    camera_buffer_upload_desc.size = sizeof(glm::mat4);
    m_resource_manager->UploadBufferData(m_camera_buffer_handle, camera_buffer_upload_desc);

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
    m_camera = std::make_unique<RendererCamera>(camera_desc);
    
    // Create shader resource
    auto vertex_shader_handle = CreateShader(RendererInterface::ShaderType::VERTEX_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainVS");
    auto fragment_shader_handle = CreateShader(RendererInterface::ShaderType::FRAGMENT_SHADER, "Resources/Shaders/ModelRenderingShader.hlsl", "MainFS");

    // Create render target resource
    auto render_target_handle = CreateRenderTarget("DemoModelViewerColorRT", m_window->GetWidth(), m_window->GetHeight(), RendererInterface::RGBA8_UNORM, RendererInterface::default_clear_color,
    static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::RENDER_TARGET | RendererInterface::ResourceUsage::COPY_SRC));

    RendererInterface::RenderPassDesc render_pass_desc{};
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::VERTEX_SHADER, vertex_shader_handle);
    render_pass_desc.shaders.emplace(RendererInterface::ShaderType::FRAGMENT_SHADER, fragment_shader_handle);

    RendererInterface::RenderTargetBindingDesc render_target_binding_desc{};
    render_target_binding_desc.format = RendererInterface::RGBA8_UNORM;
    render_target_binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR; 
    render_pass_desc.render_target_bindings.push_back(render_target_binding_desc);
    render_pass_desc.type = RendererInterface::GRAPHICS;

    auto render_pass_handle = m_resource_manager->CreateRenderPass(render_pass_desc);
    
    RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
    render_pass_draw_desc.render_target_resources.emplace(render_target_handle,
        RendererInterface::RenderTargetBindingDesc
        {
            .format = RendererInterface::RGBA8_UNORM,
            .usage = RendererInterface::RenderPassResourceUsage::COLOR,
        });
    render_pass_draw_desc.render_target_clear_states.emplace(render_target_handle, true);
    
    SceneRendererDrawDispatcher draw_dispatcher(*m_resource_manager, "glTFResources/Models/Sponza/glTF/Sponza.gltf");
    draw_dispatcher.BindDrawCommands(render_pass_draw_desc);

    // Create and bind camera view matrix buffer
    auto camera_transform = m_camera->GetProjectionMatrix();
    RendererInterface::BufferDesc camera_buffer_desc{};
    camera_buffer_desc.name = "ViewBuffer";
    camera_buffer_desc.size = sizeof(glm::mat4);
    camera_buffer_desc.type = RendererInterface::UPLOAD;
    camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
    camera_buffer_desc.data = &camera_transform;
    m_camera_buffer_handle = m_resource_manager->CreateBuffer(camera_buffer_desc);

    RendererInterface::BufferBindingDesc camera_binding_desc{};
    camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    camera_binding_desc.buffer_handle = m_camera_buffer_handle;
    camera_binding_desc.is_structured_buffer = false;
    render_pass_draw_desc.buffer_resources[camera_buffer_desc.name] = camera_binding_desc;

    RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
    render_graph_node_desc.draw_info = render_pass_draw_desc;
    render_graph_node_desc.render_pass_handle = render_pass_handle;
    render_graph_node_desc.pre_render_callback = [this](unsigned long long interval){TickFrame(interval);};

    auto render_graph_node_handle = m_render_graph->CreateRenderGraphNode(render_graph_node_desc);
    m_render_graph->RegisterRenderGraphNode(render_graph_node_handle);

    // After registration all passes, compile graph and prepare for execution
    m_render_graph->CompileRenderPassAndExecute();
    
    m_window->EnterWindowEventLoop();
}
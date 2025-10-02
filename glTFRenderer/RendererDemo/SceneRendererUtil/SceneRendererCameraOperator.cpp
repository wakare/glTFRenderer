#include "SceneRendererCameraOperator.h"

#include <glm/glm/gtx/norm.hpp>
#include "RenderWindow/RendererInputDevice.h"

SceneRendererCameraOperator::SceneRendererCameraOperator(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc)
{
    m_camera = std::make_unique<RendererCamera>(camera_desc);
    
    // Create and bind camera view matrix buffer
    auto camera_transform = m_camera->GetProjectionMatrix();
    
    camera_buffer_desc.name = "ViewBuffer";
    camera_buffer_desc.size = sizeof(glm::mat4);
    camera_buffer_desc.type = RendererInterface::UPLOAD;
    camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
    camera_buffer_desc.data = &camera_transform;
    
    m_camera_buffer_handle = resource_operator.CreateBuffer(camera_buffer_desc);
}

void SceneRendererCameraOperator::TickCameraOperation(RendererInputDevice& input_device,
    RendererInterface::ResourceOperator& resource_operator, unsigned long long delta_time_ms)
{
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
    resource_operator.UploadBufferData(m_camera_buffer_handle, camera_buffer_upload_desc);
}

void SceneRendererCameraOperator::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) const
{
    RendererInterface::BufferBindingDesc camera_binding_desc{};
    camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    camera_binding_desc.buffer_handle = m_camera_buffer_handle;
    camera_binding_desc.is_structured_buffer = false;
    out_draw_desc.buffer_resources[camera_buffer_desc.name] = camera_binding_desc;
}

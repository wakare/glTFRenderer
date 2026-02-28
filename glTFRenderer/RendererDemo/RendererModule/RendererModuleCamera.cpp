#include "RendererModuleCamera.h"

#include <algorithm>
#include <cmath>
#include <glm/glm/gtx/norm.hpp>
#include "RenderWindow/RendererInputDevice.h"

namespace
{
    float CalcViewProjectionDeltaMaxAbs(const glm::fmat4x4& lhs, const glm::fmat4x4& rhs)
    {
        float max_abs_delta = 0.0f;
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                max_abs_delta = (std::max)(max_abs_delta, std::abs(lhs[column][row] - rhs[column][row]));
            }
        }
        return max_abs_delta;
    }

    constexpr float CAMERA_CUT_MATRIX_DELTA_THRESHOLD = 0.25f;
    constexpr double CAMERA_ROTATION_CURSOR_DEADZONE_SQ = 0.04; // ~0.2 px
}

RendererModuleCamera::RendererModuleCamera(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc)
{
    m_camera = std::make_unique<RendererCamera>(camera_desc);
    
    // Create and bind camera view matrix buffer
    auto camera_transform = m_camera->GetViewProjectionMatrix();
    m_prev_view_projection_matrix = camera_transform;
    m_prev_view_projection_initialized = true;
    ViewBuffer view_buffer{};
    view_buffer.view_projection_matrix = camera_transform;
    view_buffer.prev_view_projection_matrix = m_prev_view_projection_matrix;
    view_buffer.inverse_view_projection_matrix = glm::inverse(camera_transform);
    view_buffer.view_position = glm::fvec4(m_camera->GetCameraPosition(), 1.0f);
    view_buffer.viewport_width = camera_desc.projection_width;
    view_buffer.viewport_height = camera_desc.projection_height;
    
    camera_buffer_desc.name = "ViewBuffer";
    camera_buffer_desc.size = sizeof(ViewBuffer);
    camera_buffer_desc.type = RendererInterface::UPLOAD;
    camera_buffer_desc.usage = RendererInterface::USAGE_CBV;
    camera_buffer_desc.data = &view_buffer;
    
    m_camera_buffer_handle = resource_operator.CreateBuffer(camera_buffer_desc);
}


void RendererModuleCamera::TickCameraOperation(RendererInputDevice& input_device,
                                                      unsigned long long delta_time_ms)
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
        const double cursor_offset_len_sq =
            cursor_offset.X * cursor_offset.X + cursor_offset.Y * cursor_offset.Y;
        if (cursor_offset_len_sq > CAMERA_ROTATION_CURSOR_DEADZONE_SQ)
        {
            delta_rotation.y -= static_cast<float>(cursor_offset.X);
            delta_rotation.x -= static_cast<float>(cursor_offset.Y);
            need_apply_movement = true;
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
}

bool RendererModuleCamera::FinalizeModule(RendererInterface::ResourceOperator& resource_operator)
{
    return UploadCameraViewData(resource_operator);
}

bool RendererModuleCamera::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc)
{
    RendererInterface::BufferBindingDesc camera_binding_desc{};
    camera_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    camera_binding_desc.buffer_handle = m_camera_buffer_handle;
    camera_binding_desc.is_structured_buffer = false;
    out_draw_desc.buffer_resources[camera_buffer_desc.name] = camera_binding_desc;

    return true;
}

bool RendererModuleCamera::Tick(RendererInterface::ResourceOperator& resource_operator,
    unsigned long long interval)
{
    RETURN_IF_FALSE(RendererModuleBase::Tick(resource_operator, interval))
    RETURN_IF_FALSE(UploadCameraViewData(resource_operator))
    
    return true;
}

bool RendererModuleCamera::SetViewportSize(unsigned width, unsigned height)
{
    if (!m_camera || width == 0 || height == 0)
    {
        return false;
    }

    const unsigned current_width = static_cast<unsigned>(m_camera->GetProjectionWidth());
    const unsigned current_height = static_cast<unsigned>(m_camera->GetProjectionHeight());
    if (current_width == width && current_height == height)
    {
        return false;
    }

    m_camera->SetProjectionSize(static_cast<float>(width), static_cast<float>(height));
    m_prev_view_projection_initialized = false;
    m_temporal_history_invalidation_pending = true;
    return true;
}

unsigned RendererModuleCamera::GetWidth() const
{
    return m_camera->GetProjectionWidth();
}

unsigned RendererModuleCamera::GetHeight() const
{
    return m_camera->GetProjectionHeight();
}

bool RendererModuleCamera::ConsumeTemporalHistoryInvalidation()
{
    const bool invalidation_pending = m_temporal_history_invalidation_pending;
    m_temporal_history_invalidation_pending = false;
    return invalidation_pending;
}

bool RendererModuleCamera::UploadCameraViewData(RendererInterface::ResourceOperator& resource_operator)
{
    const auto camera_transform = m_camera->GetViewProjectionMatrix();
    if (!m_prev_view_projection_initialized)
    {
        m_temporal_history_invalidation_pending = true;
    }
    else
    {
        const float matrix_delta = CalcViewProjectionDeltaMaxAbs(camera_transform, m_prev_view_projection_matrix);
        if (matrix_delta > CAMERA_CUT_MATRIX_DELTA_THRESHOLD)
        {
            m_temporal_history_invalidation_pending = true;
        }
    }

    const auto prev_view_projection = m_prev_view_projection_initialized ? m_prev_view_projection_matrix : camera_transform;

    ViewBuffer view_buffer{};
    view_buffer.view_projection_matrix = camera_transform;
    view_buffer.prev_view_projection_matrix = prev_view_projection;
    view_buffer.inverse_view_projection_matrix = glm::inverse(camera_transform);
    view_buffer.view_position = glm::fvec4(m_camera->GetCameraPosition(), 1.0f);
    view_buffer.viewport_width = m_camera->GetProjectionWidth();
    view_buffer.viewport_height = m_camera->GetProjectionHeight();
    
    RendererInterface::BufferUploadDesc camera_buffer_upload_desc{};
    camera_buffer_upload_desc.data = &view_buffer;
    camera_buffer_upload_desc.size = sizeof(ViewBuffer);
    resource_operator.UploadBufferData(m_camera_buffer_handle, camera_buffer_upload_desc);

    m_prev_view_projection_matrix = camera_transform;
    m_prev_view_projection_initialized = true;
    return true;
}

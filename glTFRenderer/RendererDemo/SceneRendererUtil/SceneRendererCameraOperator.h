#pragma once
#include <memory>

#include "Renderer.h"
#include "RendererCamera.h"
#include "RendererInterface.h"

class SceneRendererCameraOperator
{
public:
    SceneRendererCameraOperator(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc);
    void TickCameraOperation(RendererInputDevice& input_device, RendererInterface::ResourceOperator& resource_operator, unsigned long long delta_time_ms);
    void BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) const;
    
protected:
    RendererInterface::BufferDesc camera_buffer_desc{};
    std::unique_ptr<RendererCamera> m_camera;
    RendererInterface::BufferHandle m_camera_buffer_handle{NULL_HANDLE};
};

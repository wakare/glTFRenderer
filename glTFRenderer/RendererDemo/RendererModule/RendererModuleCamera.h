#pragma once
#include <memory>

#include "Renderer.h"
#include "RendererCamera.h"
#include "RendererInterface.h"

struct ViewBuffer
{
    glm::fmat4x4 view_projection_matrix;
    glm::fmat4x4 prev_view_projection_matrix;
    glm::fmat4x4 inverse_view_projection_matrix;
    glm::fvec4 view_position;
    
    unsigned viewport_width;
    unsigned viewport_height;
    unsigned padding[2];
};

class RendererModuleCamera : public RendererInterface::RendererModuleBase
{
public:
    RendererModuleCamera(RendererInterface::ResourceOperator& resource_operator, const RendererCameraDesc& camera_desc);
    void TickCameraOperation(RendererInputDevice& input_device, unsigned long long delta_time_ms);
    
    virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) override;
    bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) override;
    virtual bool Tick(RendererInterface::ResourceOperator&, unsigned long long interval) override;

    bool SetViewportSize(unsigned width, unsigned height);
    unsigned GetWidth() const;
    unsigned GetHeight() const;
    
protected:
    bool UploadCameraViewData(RendererInterface::ResourceOperator& resource_operator);
    
    RendererInterface::BufferDesc camera_buffer_desc{};
    std::unique_ptr<RendererCamera> m_camera;
    RendererInterface::BufferHandle m_camera_buffer_handle{NULL_HANDLE};
    glm::fmat4x4 m_prev_view_projection_matrix{1.0f};
    bool m_prev_view_projection_initialized{false};
};

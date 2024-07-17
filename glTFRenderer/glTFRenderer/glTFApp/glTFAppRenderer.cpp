#include "glTFAppRenderer.h"
#include "glTFGUIRenderer.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "RenderWindow/glTFWindow.h"

glTFAppRenderer::glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window)
{
    RHIConfigSingleton::Instance().SetGraphicsAPIType(renderer_config.vulkan ?
        RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);

    if (renderer_config.test_triangle)
    {
        m_scene_renderer.reset(new glTFSceneRendererTestTriangle);   
    }
    else
    {
        if (renderer_config.raster)
        {
            m_scene_renderer.reset(new glTFSceneRendererRasterizer);    
        }
        else
        {
            m_scene_renderer.reset(new glTFSceneRendererRayTracer(renderer_config.ReSTIR));
        }    
    }

    m_resource_manager.reset(new glTFRenderResourceManager());
    m_resource_manager->InitResourceManager(window.GetWidth(), window.GetHeight(), window.GetHWND());
    
    m_ui_renderer = std::make_unique<glTFGUIRenderer>();
    m_ui_renderer->SetupGUIContext(glTFWindow::Get(), GetResourceManager());
    m_ui_renderer->AddWidgetSetupCallback([this]()
    {
        m_scene_renderer->TickGUIWidgetUpdate(*m_ui_renderer, *m_resource_manager, 0);
    });
}

bool glTFAppRenderer::InitScene(const glTFSceneGraph& scene_graph)
{
    m_scene_view.reset(new glTFSceneView(scene_graph));
    return m_resource_manager->InitScene(scene_graph);
}

void glTFAppRenderer::TickRenderingBegin(size_t delta_time_ms)
{
    m_scene_renderer->TickFrameRenderingBegin(*m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneUpdating(const glTFSceneGraph& scene_graph,const glTFInputManager& input_manager, size_t delta_time_ms)
{
    m_scene_view->Tick(scene_graph);
    m_resource_manager->GetRadiosityRenderer().UpdateIndirectLighting(scene_graph, m_scene_view->GetLightingDirty());

    m_scene_view->ApplyInput(input_manager, delta_time_ms);
    m_scene_renderer->ApplyInput(input_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneRendering(const glTFInputManager& input_manager, size_t delta_time_ms)
{
    m_scene_renderer->TickSceneRendering(*m_scene_view, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickGUIWidgetUpdate(size_t delta_time_ms)
{
    m_ui_renderer->UpdateWidgets();
    m_ui_renderer->RenderWidgets(*m_resource_manager);
}

void glTFAppRenderer::TickRenderingEnd(size_t delta_time_ms)
{
    m_scene_renderer->TickFrameRenderingEnd(*m_resource_manager, delta_time_ms);
}

glTFGUIRenderer& glTFAppRenderer::GetGUIRenderer() const
{
    return *m_ui_renderer;
}

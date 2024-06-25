#include "glTFAppRenderer.h"
#include "glTFGUI/glTFGUI.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFWindow/glTFWindow.h"

glTFAppRenderer::glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window)
{
    RHIConfigSingleton::Instance().SetGraphicsAPIType(renderer_config.vulkan ?
        RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);

    if (renderer_config.test_triangle_pass)
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineTestTriangle);   
    }
    else
    {
        if (renderer_config.raster)
        {
            m_render_pipeline.reset(new glTFAppRenderPipelineRasterScene);    
        }
        else
        {
            m_render_pipeline.reset(new glTFAppRenderPipelineRayTracingScene(renderer_config.ReSTIR));
        }    
    }

    m_resource_manager.reset(new glTFRenderResourceManager());
    m_resource_manager->InitResourceManager(window.GetWidth(), window.GetHeight(), window.GetHWND());
}

bool glTFAppRenderer::InitGUIContext(glTFGUI& GUI, const glTFWindow& window) const
{
    return GUI.SetupGUIContext(window, *m_resource_manager);
}

bool glTFAppRenderer::InitScene(const glTFSceneGraph& scene_graph)
{
    m_scene_view.reset(new glTFSceneView(scene_graph));
    return m_resource_manager->InitScene(scene_graph);
}

void glTFAppRenderer::TickRenderingBegin(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingBegin(*m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneUpdating(const glTFSceneGraph& scene_graph, size_t delta_time_ms)
{
    m_scene_view->Tick(scene_graph);
    m_resource_manager->GetRadiosityRenderer().UpdateIndirectLighting(scene_graph, m_scene_view->GetLightingDirty());
}

void glTFAppRenderer::TickSceneRendering(const glTFInputManager& input_manager, size_t delta_time_ms)
{
    m_scene_view->ApplyInput(input_manager, delta_time_ms);
    m_render_pipeline->ApplyInput(input_manager, delta_time_ms);
    m_render_pipeline->TickSceneRendering(*m_scene_view, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickGUIWidgetUpdate(glTFGUI& GUI, size_t delta_time_ms)
{
    m_render_pipeline->TickGUIWidgetUpdate(GUI, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickRenderingEnd(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingEnd(*m_resource_manager, delta_time_ms);
}

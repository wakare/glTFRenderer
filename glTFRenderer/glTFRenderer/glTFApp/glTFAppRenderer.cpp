#include "glTFAppRenderer.h"

#include "glTFGUI/glTFGUI.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFWindow/glTFWindow.h"

glTFAppRenderer::glTFAppRenderer(bool raster_scene, const glTFWindow& window)
{
    if (raster_scene)
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRasterScene);    
    }
    else
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRayTracingScene);
    }

    m_resource_manager.reset(new glTFRenderResourceManager());
    m_resource_manager->InitResourceManager(window.GetWidth(), window.GetHeight(), window.GetHWND());
}

bool glTFAppRenderer::InitGUIContext(glTFGUI& GUI, const glTFWindow& window) const
{
    return GUI.SetupGUIContext(window, *m_resource_manager);
}

void glTFAppRenderer::TickRenderingBegin(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingBegin(*m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneRendering(const glTFSceneGraph& scene_graph, const glTFInputManager& input_manager, size_t delta_time_ms)
{
    if (!m_scene_view)
    {
        m_scene_view.reset(new glTFSceneView(scene_graph));    
    }
    else
    {
        m_scene_view->Tick(scene_graph);
    }    
    
    input_manager.TickSceneView(*m_scene_view, delta_time_ms);
    input_manager.TickRenderPipeline(*m_render_pipeline, delta_time_ms);
    
    m_render_pipeline->TickSceneRendering(scene_graph, *m_scene_view, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickGUIFrameRendering(glTFGUI& GUI, size_t delta_time_ms)
{
    m_render_pipeline->TickGUIRendering(GUI, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickRenderingEnd(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingEnd(*m_resource_manager, delta_time_ms);
}

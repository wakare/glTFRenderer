#include "glTFAppSceneRenderer.h"

#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFWindow/glTFWindow.h"

glTFAppSceneRenderer::glTFAppSceneRenderer(bool raster_scene)
{
    if (raster_scene)
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRasterScene);    
    }
    else
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRayTracingScene);
    }
}

void glTFAppSceneRenderer::TickFrame(const glTFSceneGraph& scene_graph, const glTFInputManager& input_manager, size_t delta_time_ms)
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
    
    m_render_pipeline->Tick(scene_graph, *m_scene_view, delta_time_ms);
}

#pragma once
#include "glTFAppRenderPipeline.h"

class glTFAppSceneRenderer
{
public:
    glTFAppSceneRenderer(bool raster_scene);
    void TickFrame(const glTFSceneGraph& scene_graph, const glTFInputManager& input_manager, size_t delta_time_ms);
    
protected:
    std::unique_ptr<glTFAppRenderPipelineBase> m_render_pipeline;
    std::unique_ptr<glTFSceneView> m_scene_view;
};

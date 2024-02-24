#pragma once
#include "glTFAppRenderPipeline.h"

struct glTFAppRendererConfig
{
    bool raster;
    bool ReSTIR;
    bool vulkan;
    bool test_triangle_pass;

    glTFAppRendererConfig()
        : raster(true) 
        , ReSTIR(false)
        , vulkan(false)
        , test_triangle_pass(false)
    {
        
    }
};

class glTFAppRenderer
{
public:
    glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window);
    bool InitGUIContext(glTFGUI& GUI, const glTFWindow& window) const;
    bool InitMeshResourceWithSceneGraph(const glTFSceneGraph& scene_graph);
    
    void TickRenderingBegin(size_t delta_time_ms);
    void TickSceneUpdating(const glTFSceneGraph& scene_graph, size_t delta_time_ms);
    void TickSceneRendering(const glTFInputManager& input_manager, size_t delta_time_ms);
    void TickGUIWidgetUpdate(glTFGUI& GUI, size_t delta_time_ms);
    void TickRenderingEnd(size_t delta_time_ms);

    glTFRenderResourceManager& GetResourceManager() const {return *m_resource_manager; }
    
protected:
    std::unique_ptr<glTFAppRenderPipelineBase> m_render_pipeline;
    std::unique_ptr<glTFSceneView> m_scene_view;
    std::unique_ptr<glTFRenderResourceManager> m_resource_manager;

    
};

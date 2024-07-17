#pragma once
#include "glTFSceneRenderer.h"

struct glTFAppRendererConfig
{
    bool raster;
    bool ReSTIR;
    bool vulkan;

    // test sample
    bool test_triangle;

    glTFAppRendererConfig()
        : raster(true) 
        , ReSTIR(false)
        , vulkan(false)
        , test_triangle(true)
    {
        
    }
};

class glTFAppRenderer
{
public:
    glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window);
    bool InitScene(const glTFSceneGraph& scene_graph);
    
    void TickRenderingBegin(size_t delta_time_ms);
    void TickSceneUpdating(const glTFSceneGraph& scene_graph, const glTFInputManager& input_manager, size_t delta_time_ms);
    void TickSceneRendering(const glTFInputManager& input_manager, size_t delta_time_ms);
    void TickGUIWidgetUpdate(size_t delta_time_ms);
    void TickRenderingEnd(size_t delta_time_ms);

    glTFGUIRenderer& GetGUIRenderer() const;
    glTFRenderResourceManager& GetResourceManager() const {return *m_resource_manager; }
    
protected:
    std::unique_ptr<glTFSceneRendererBase> m_scene_renderer;
    std::unique_ptr<glTFGUIRenderer> m_ui_renderer;
    std::unique_ptr<glTFSceneView> m_scene_view;
    std::unique_ptr<glTFRenderResourceManager> m_resource_manager;
};

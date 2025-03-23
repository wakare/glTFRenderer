#pragma once
#include "glTFSceneRenderer.h"

struct glTFAppRendererConfig
{
    bool raster;
    bool ReSTIR;
    bool vulkan;
    bool ui;
    bool virtual_texture;
    bool shadow;
    
    // test sample
    bool test_triangle;

    glTFAppRendererConfig()
        : raster(true)
        , ReSTIR(false)
        , vulkan(false)
        , ui(false)
        , virtual_texture(false)
        , shadow(true)
        , test_triangle(true)
    {
    }
};

class glTFAppRenderer
{
public:
    glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window);
    bool InitScene(std::shared_ptr<glTFSceneGraph> scene_graph);
    
    void TickRenderingBegin(size_t delta_time_ms);
    void TickSceneUpdating(const glTFSceneGraph& scene_graph, const glTFInputManager& input_manager, size_t delta_time_ms);
    void TickSceneRendering(const glTFInputManager& input_manager, const glTFSceneGraph& scene_graph, size_t delta_time_ms);
    void TickGUIWidgetUpdate(size_t delta_time_ms);
    void TickRenderingEnd(size_t delta_time_ms);

    void WaitRenderingFinishAndCleanupAllResource() const;
    
    glTFGUIRenderer& GetGUIRenderer() const;
    glTFRenderResourceManager& GetResourceManager() const {return *m_resource_manager; }
    
protected:
    std::unique_ptr<glTFSceneRendererBase> m_scene_renderer;
    std::unique_ptr<glTFGUIRenderer> m_ui_renderer;
    std::unique_ptr<glTFSceneView> m_scene_view;
    std::unique_ptr<glTFRenderResourceManager> m_resource_manager;

    std::vector<std::shared_ptr<RenderSystemBase>> m_render_systems;
};

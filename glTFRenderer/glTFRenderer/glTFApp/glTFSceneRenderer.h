#pragma once
#include "glTFPassOptionRenderFlags.h"
#include "glTFGUIRenderer.h"
#include "glTFRenderPass/glTFRenderPassManager.h"
#include "glTFRenderSystem/RenderSystemBase.h"

class glTFRenderResourceManager;
class RendererInputDevice;
class glTFSceneView;
class glTFSceneGraph;

class glTFSceneRendererBase
{
public:
    glTFSceneRendererBase();
    virtual ~glTFSceneRendererBase();

    virtual bool SetupSceneRenderer(const glTFSceneGraph& scene_graph) = 0;

    virtual void TickFrameRenderingBegin(glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickSceneUpdating(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickSceneRendering(glTFRenderResourceManager& resource_manager, const glTFSceneView& scene_view, const glTFSceneGraph& scene_graph, size_t delta_time_ms);
    virtual void TickGUIWidgetUpdate(glTFGUIRenderer& GUI, glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickFrameRenderingEnd(glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    
    void ApplyInput(const RendererInputDevice& input_manager, size_t delta_time_ms);
    glTFRenderPassManager& GetPassManager();
    
protected:
    bool RecreateRenderPass(glTFRenderResourceManager& resource_manager, const glTFSceneGraph& scene_graph);
    virtual bool UpdateGUIWidgets() { return true; }
    
    glTFPassOptionRenderFlags m_pass_options;
    std::unique_ptr<glTFRenderPassManager> m_pass_manager;
    bool m_pass_inited {false};
};

class glTFSceneRendererRasterizer : public glTFSceneRendererBase
{
public:
    virtual bool SetupSceneRenderer(const glTFSceneGraph& scene_graph) override;
};

class glTFSceneRendererRayTracer : public glTFSceneRendererBase
{
public:
    glTFSceneRendererRayTracer(bool use_restir_direct_lighting);

    virtual bool SetupSceneRenderer(const glTFSceneGraph& scene_graph) override;

protected:
    bool m_use_restir_direct_lighting;
};

class glTFSceneRendererTestTriangle : public glTFSceneRendererBase
{
public:
    virtual bool SetupSceneRenderer(const glTFSceneGraph& scene_graph) override;
};
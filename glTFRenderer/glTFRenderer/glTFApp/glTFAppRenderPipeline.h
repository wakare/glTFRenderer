#pragma once
#include "glTFPassOptionRenderFlags.h"
#include "glTFRenderPass/glTFRenderPassManager.h"

class glTFRenderResourceManager;
class glTFInputManager;
class glTFSceneView;
class glTFSceneGraph;
class glTFGUI;

class glTFAppRenderPipelineBase
{
public:
    glTFAppRenderPipelineBase();
    virtual ~glTFAppRenderPipelineBase() {m_pass_manager->ExitAllPass(); }

    virtual bool SetupRenderPipeline() = 0;

    void MarkPipelineDirty() { m_render_pass_dirty = true; }

    virtual void TickFrameRenderingBegin(glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickSceneRendering(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickGUIRendering(glTFGUI& GUI, glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    virtual void TickFrameRenderingEnd(glTFRenderResourceManager& resource_manager, size_t delta_time_ms);
    
    void ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms);

protected:
    bool RecreateRenderPass(glTFRenderResourceManager& resource_manager);
    
    bool m_render_pass_dirty;
    
    glTFPassOptionRenderFlags m_pass_options;
    std::unique_ptr<glTFRenderPassManager> m_pass_manager;
};

class glTFAppRenderPipelineRasterScene : public glTFAppRenderPipelineBase
{
public:
    virtual bool SetupRenderPipeline() override;
};

class glTFAppRenderPipelineRayTracingScene : public glTFAppRenderPipelineBase
{
public:
    glTFAppRenderPipelineRayTracingScene();

    virtual bool SetupRenderPipeline() override;

protected:
    bool use_restir_direct_lighting;
};
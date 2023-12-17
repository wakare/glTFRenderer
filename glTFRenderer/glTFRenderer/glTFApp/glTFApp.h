#pragma once
#include "glTFAppRenderPipeline.h"
#include "glTFRenderPass/glTFRenderPassManager.h"

class glTFTimer
{
public:
    glTFTimer();
    void RecordFrameBegin();
    size_t GetDeltaFrameTimeMs() const;

private:
    size_t m_delta_tick;
    size_t m_tick;
};

class glTFApp
{
public:
    bool InitApp();
    void RunApp();
    void ExitApp() const;

protected:
    void TickFrame();
    bool LoadSceneGraphFromFile(const char* filePath) const;
    
    glTFTimer m_timer;
    std::shared_ptr<glTFInputManager> m_input_manager;

    std::unique_ptr<glTFAppRenderPipelineBase> m_render_pipeline;
    std::unique_ptr<glTFSceneView> m_scene_view;
    std::unique_ptr<glTFSceneGraph> m_scene_graph;
};

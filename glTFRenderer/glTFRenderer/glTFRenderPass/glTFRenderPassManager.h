#pragma once

#include <memory>

#include "glTFRenderPassBase.h"
#include "glTFRenderResourceManager.h"
#include "glTFScene/glTFSceneView.h"
#include "glTFApp/glTFPassOptionRenderFlags.h"

enum RenderPassPhaseType
{
    PRE_SCENE = 0,
    SCENE_DEPTH = 1,
    SCENE_BASE_PASS = 2,
    SCENE_SHADOW = 3,
    SCENE_LIGHTING = 4,
    POST_SCENE = 5,
};


class glTFRenderPassManager
{
public:
    glTFRenderPassManager();
    
    bool InitRenderPassManager(glTFRenderResourceManager& resource_manager);
    void AddRenderPass(RenderPassPhaseType type, std::shared_ptr<glTFRenderPassBase> pass);

    void InitAllPass(glTFRenderResourceManager& resource_manager);
    void UpdateScene(glTFRenderResourceManager& resource_manager, const glTFSceneView& scene_view, size_t delta_time_ms);
    void UpdateAllPassGUIWidgets();
    
    void RenderBegin(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs);
    void RenderAllPass(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs) const;
    void RenderEnd(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs);
    
    void UpdatePipelineOptions(const glTFPassOptionRenderFlags& pipeline_options);
    void ExitAllPass();

protected:
    void TraveseAllPass(std::function<void(glTFRenderPassBase& pass)> lambda) const;
    
    std::map<RenderPassPhaseType, std::vector<std::shared_ptr<glTFRenderPassBase>>> m_passes;
    unsigned m_frame_index;

    std::shared_ptr<IRHIRenderPass> m_render_pass;
    std::vector<std::shared_ptr<IRHIFrameBuffer>> m_frame_buffers;
};

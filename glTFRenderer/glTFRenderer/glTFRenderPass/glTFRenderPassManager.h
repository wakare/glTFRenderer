#pragma once
#include <memory>

#include "glTFRenderPassBase.h"
#include "glTFRenderResourceManager.h"
#include "../glTFScene/glTFSceneGraph.h"
#include "../glTFScene/glTFSceneView.h"
#include "glTFApp/glTFPassOptionRenderFlags.h"

class glTFRenderPassManager
{
public:
    glTFRenderPassManager(const glTFSceneView& view);
    
    bool InitRenderPassManager(const glTFWindow& window);
    void AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass);

    void InitAllPass(glTFRenderResourceManager& resource_manager);
    void UpdateScene(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs);
    void UpdateAllPassGUIWidgets();
    
    void RenderBegin(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs);
    void RenderAllPass(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs) const;
    void RenderEnd(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs);
    
    void UpdatePipelineOptions(const glTFPassOptionRenderFlags& pipeline_options);
    void ExitAllPass();

protected:
    const glTFSceneView& m_scene_view;
    
    std::vector<std::unique_ptr<glTFRenderPassBase>> m_passes;
    
    unsigned m_frame_index;
};

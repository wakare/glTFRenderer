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

    void InitAllPass();

    void UpdateScene(size_t deltaTimeMs);
    void UpdatePipelineOptions(const glTFPassOptionRenderFlags& pipeline_options);
    void RenderAllPass(size_t deltaTimeMs) const;
    void ExitAllPass();

    glTFRenderResourceManager& GetResourceManager() {return *m_resource_manager; }
    
protected:
    const glTFSceneView& m_scene_view;
    
    std::vector<std::unique_ptr<glTFRenderPassBase>> m_passes;
    std::unique_ptr<glTFRenderResourceManager> m_resource_manager;
    
    unsigned m_frame_index;
};

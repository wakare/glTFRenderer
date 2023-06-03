#pragma once
#include <memory>

#include "glTFRenderPassBase.h"
#include "glTFRenderResourceManager.h"
#include "../glTFScene/glTFSceneGraph.h"
#include "../glTFScene/glTFSceneView.h"

class glTFRenderPassManager
{
public:
    glTFRenderPassManager(glTFWindow& window, glTFSceneView& view);
    
    bool InitRenderPassManager();
    void AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass);

    void InitAllPass();

    void UpdateScene(size_t deltaTimeMs);
    void RenderAllPass(size_t deltaTimeMs);
    void ExitAllPass();

protected:
    void ExecuteCommandSection(IRHIPipelineStateObject* PSO, std::function<void()> executeLambda);
    
    glTFWindow& m_window;
    glTFSceneView& m_sceneView;
    
    std::vector<std::unique_ptr<glTFRenderPassBase>> m_passes;
    std::unique_ptr<glTFRenderResourceManager> m_resourceManager;
    
    unsigned m_frameIndex;
};

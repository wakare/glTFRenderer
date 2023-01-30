#pragma once
#include <memory>

#include "glTFRenderPassBase.h"
#include "glTFRenderResourceManager.h"

class glTFRenderPassManager
{
public:
    glTFRenderPassManager(glTFWindow& window);
    
    bool InitRenderPassManager();
    void AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass);

    void InitAllPass();
    void RenderAllPass();
    void ExitAllPass();
    
protected:
    glTFWindow& m_window;
    
    std::vector<std::unique_ptr<glTFRenderPassBase>> m_passes;
    std::unique_ptr<glTFRenderResourceManager> m_resourceManager;
    
    unsigned m_frameIndex;
};

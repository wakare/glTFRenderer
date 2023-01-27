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
    
protected:
    glTFWindow& m_window;
    
    std::unique_ptr<glTFRenderResourceManager> m_resourceManager;
    std::vector<std::unique_ptr<glTFRenderPassBase>> m_passes;
};

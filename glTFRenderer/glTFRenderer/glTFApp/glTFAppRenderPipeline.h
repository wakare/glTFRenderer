#pragma once
#include "glTFRenderPass/glTFRenderPassManager.h"

class glTFAppRenderPipeline
{
public:
    bool SetupRenderPipeline();
    
    void Tick();
    
protected:
    std::unique_ptr<glTFRenderPassManager> m_pass_manager;
};

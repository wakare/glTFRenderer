#pragma once
#include "glTFRenderResourceManager.h"
#include "../glTFMaterial/glTFMaterialBase.h"

class IRHICommandList;

class glTFRenderPassBase 
{
public:
    glTFRenderPassBase() = default;
    
    glTFRenderPassBase(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase(glTFRenderPassBase&&) = delete;

    glTFRenderPassBase& operator=(const glTFRenderPassBase&) = delete;
    glTFRenderPassBase& operator=(glTFRenderPassBase&&) = delete;

    // Which material should process within render pass
    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager, const glTFMaterialBase& material) {return false;} 
    
    virtual ~glTFRenderPassBase() = 0;
    virtual const char* PassName() = 0;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) = 0;
};

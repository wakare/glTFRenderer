#pragma once
#include "RendererCommon.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"

class RenderSystemBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RenderSystemBase)
    
    virtual bool InitRenderSystem(glTFRenderResourceManager& resource_manager) = 0;
    virtual void ShutdownRenderSystem() = 0;
    virtual void TickRenderSystem(glTFRenderResourceManager& resource_manager) = 0;
    
};

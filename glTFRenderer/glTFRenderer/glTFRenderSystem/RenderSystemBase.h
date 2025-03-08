#pragma once
#include "RendererCommon.h"

class RenderSystemBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RenderSystemBase)
    
    virtual bool InitRenderSystem() = 0;
    virtual void ShutdownRenderSystem() = 0;
    virtual void TickRenderSystem() = 0;
    
};

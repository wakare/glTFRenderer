#pragma once
#include "glTFRenderResourceManager.h"

class IRHICommandList;

class glTFRenderPassBase 
{
public:
    virtual const char* PassName() = 0;
    virtual bool InitRenderPass(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) = 0;
};

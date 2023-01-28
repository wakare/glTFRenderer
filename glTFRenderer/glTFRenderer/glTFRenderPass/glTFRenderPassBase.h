#pragma once
#include "glTFRenderResourceManager.h"

class IRHICommandList;

class glTFRenderPassBase 
{
public:
    virtual ~glTFRenderPassBase() = default;
    virtual const char* PassName() = 0;
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) = 0;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) = 0;
};

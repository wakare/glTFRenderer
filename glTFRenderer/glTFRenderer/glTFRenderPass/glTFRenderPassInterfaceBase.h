#pragma once
#include "glTFRenderResourceManager.h"

// glTF render pass interface can composite with render pass class, should be derived in class declaration
class glTFRenderPassInterfaceBase
{
public:
    virtual bool InitInterface(glTFRenderResourceManager& resourceManager) = 0;
    
    virtual ~glTFRenderPassInterfaceBase() = default;
};

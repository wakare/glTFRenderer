#pragma once
#include "glTFRenderResourceUtils.h"

class glTFRenderResourceFrameManager
{
public:
    glTFRenderResourceFrameManager();
    
    glTFRenderResourceUtils::GBufferOutput& GetGBufferForInit();
    const glTFRenderResourceUtils::GBufferOutput& GetGBufferForRendering() const;
    
protected:
    glTFRenderResourceUtils::GBufferOutput m_GBuffer;
};

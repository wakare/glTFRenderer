#pragma once
#include "glTFRenderPassBase.h"

class IRHIIndexBufferView;
class IRHIVertexBufferView;
class IRHIGPUBuffer;

struct PostprocessQuadGPUResource
{
    std::shared_ptr<IRHIGPUBuffer> meshVertexBuffer;
    std::shared_ptr<IRHIGPUBuffer> meshIndexBuffer;
    std::shared_ptr<IRHIVertexBufferView> meshVertexBufferView;
    std::shared_ptr<IRHIIndexBufferView> meshIndexBufferView;
};

// Post process pass only draw full screen quad
class glTFRenderPassPostprocess : public glTFRenderPassBase
{
public:
    glTFRenderPassPostprocess();

    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    
protected:
    void DrawPostprocessQuad(glTFRenderResourceManager& resourceManager);
    
private:
    PostprocessQuadGPUResource m_postprocessQuadResource;
};

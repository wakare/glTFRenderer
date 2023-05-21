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
    // Must be implement in final render pass class
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    void DrawPostprocessQuad(glTFRenderResourceManager& resourceManager);
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;
    
private:
    PostprocessQuadGPUResource m_postprocessQuadResource;
};

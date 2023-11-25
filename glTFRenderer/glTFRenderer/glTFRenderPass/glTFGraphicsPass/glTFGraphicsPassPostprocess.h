#pragma once
#include "glTFGraphicsPassBase.h"
#include "glTFRenderPass/glTFRenderPassBase.h"

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
class glTFGraphicsPassPostprocess : public glTFGraphicsPassBase
{
public:
    glTFGraphicsPassPostprocess();

    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    // Must be implement in final render pass class
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    void DrawPostprocessQuad(glTFRenderResourceManager& resourceManager);
    virtual const std::vector<RHIPipelineInputLayout>&
    GetVertexInputLayout(glTFRenderResourceManager& resource_manager) override;
    
private:
    PostprocessQuadGPUResource m_postprocessQuadResource;
};

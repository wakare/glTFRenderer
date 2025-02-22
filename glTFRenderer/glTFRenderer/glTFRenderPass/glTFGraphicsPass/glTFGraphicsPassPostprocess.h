#pragma once
#include "glTFGraphicsPassMeshBase.h"
#include "glTFRHI/RHIVertexStreamingManager.h"

class IRHIBufferAllocation;
class IRHIIndexBufferView;
class IRHIVertexBufferView;
class IRHIBuffer;

struct PostprocessQuadGPUResource
{
    std::shared_ptr<IRHIBufferAllocation> meshVertexBuffer;
    std::shared_ptr<IRHIBufferAllocation> meshIndexBuffer;
    std::shared_ptr<IRHIVertexBufferView> meshVertexBufferView;
    std::shared_ptr<IRHIIndexBufferView> meshIndexBufferView;
};

// Post process pass only draw full screen quad
class glTFGraphicsPassPostprocess : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassPostprocess();

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    // Must be implement in final render pass class
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;
    
    void DrawPostprocessQuad(glTFRenderResourceManager& resourceManager);
    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const override;
    
private:
    PostprocessQuadGPUResource m_postprocessQuadResource;
    RHIVertexStreamingManager m_vertex_streaming_manager;
};

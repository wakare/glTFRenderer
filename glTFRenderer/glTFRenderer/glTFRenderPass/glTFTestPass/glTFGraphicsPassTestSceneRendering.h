#pragma once
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassBase.h"
#include "RHIVertexStreamingManager.h"

class IRHICommandSignature;

class glTFGraphicsPassTestSceneRendering : public glTFGraphicsPassBase
{
public:
    glTFGraphicsPassTestSceneRendering();
    virtual const char* PassName() override {return "TestSceneRendering"; }
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    
    bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool RenderPass(glTFRenderResourceManager& resource_manager) override;

protected:
    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const override;

    std::shared_ptr<IRHICommandSignature> m_command_signature;
    bool m_indirect_draw{true};
    RHIVertexStreamingManager m_dummy_vertex_streaming_manager;
};

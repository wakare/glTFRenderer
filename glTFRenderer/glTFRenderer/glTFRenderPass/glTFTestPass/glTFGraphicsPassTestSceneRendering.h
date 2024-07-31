#pragma once
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassBase.h"

class glTFGraphicsPassTestSceneRendering : public glTFGraphicsPassBase
{
public:
    glTFGraphicsPassTestSceneRendering();
    virtual const char* PassName() override {return "TestSceneRendering"; }
    
    bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool RenderPass(glTFRenderResourceManager& resource_manager) override;

protected:
    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const override;
};

#pragma once
#include "glTFGraphicsPassBase.h"

class glTFGraphicsPassTestTriangle : public glTFGraphicsPassBase
{
public:
    glTFGraphicsPassTestTriangle();
    
    virtual const char* PassName() override;
    
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
};

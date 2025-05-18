#pragma once
#include "glTFGraphicsPassTestTriangleBase.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassBase.h"

class glTFGraphicsPassTestTriangleSimplest : public glTFGraphicsPassTestTriangleBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassTestTriangleSimplest)
    
    virtual const char* PassName() override;
    
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
};

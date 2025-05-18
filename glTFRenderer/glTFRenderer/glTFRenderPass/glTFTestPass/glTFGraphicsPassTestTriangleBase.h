#pragma once
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassBase.h"

class glTFGraphicsPassTestTriangleBase : public glTFGraphicsPassBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassTestTriangleBase)
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
};

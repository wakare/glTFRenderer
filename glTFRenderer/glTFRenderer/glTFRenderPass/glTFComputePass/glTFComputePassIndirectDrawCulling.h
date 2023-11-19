#pragma once
#include "glTFComputePassBase.h"

class glTFComputePassIndirectDrawCulling : public glTFComputePassBase
{
public:
    glTFComputePassIndirectDrawCulling();
    
    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual DispatchCount GetDispatchCount() const override;
    
protected:
    bool InitVertexAndInstanceBufferForFrame(glTFRenderResourceManager& resource_manager);

    DispatchCount m_dispatch_count;
};

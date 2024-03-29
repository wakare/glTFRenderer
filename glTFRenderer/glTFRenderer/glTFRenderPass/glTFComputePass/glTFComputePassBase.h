#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

struct DispatchCount
{
    unsigned X;
    unsigned Y;
    unsigned Z;
};

class glTFComputePassBase : public glTFRenderPassBase
{
public:
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual DispatchCount GetDispatchCount() const = 0; 
    
protected:
    IRHIComputePipelineStateObject& GetComputePipelineStateObject() const;
    
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual PipelineType GetPipelineType() const override {return PipelineType::Compute; }
};

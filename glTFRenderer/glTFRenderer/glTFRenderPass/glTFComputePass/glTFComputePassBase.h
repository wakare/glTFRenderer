#pragma once
#include "glTFRenderPass/glTFRenderPassBase.h"

class IRHIComputePipelineStateObject;

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
    virtual RHIPipelineType GetPipelineType() const override {return RHIPipelineType::Compute; }
    using glTFRenderPassBase::InitResourceTable;
};

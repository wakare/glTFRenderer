#pragma once
#include "glTFRenderPass/glTFComputePass/glTFComputePassBase.h"

class glTFComputePassTestFillColor : public glTFComputePassBase
{
public:
    virtual const char* PassName() override;
    
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;
    
protected:
    RootSignatureAllocation m_output_allocation;
};

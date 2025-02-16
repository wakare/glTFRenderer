#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"

class glTFComputePassLighting : public glTFComputePassBase
{
public:
    glTFComputePassLighting();

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount() const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    virtual bool ModifyFinalOutput(RenderGraphNodeUtil::RenderGraphNodeFinalOutput& final_output) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    DispatchCount m_dispatch_count;

    RootSignatureAllocation m_albedo_allocation;
    RootSignatureAllocation m_depth_allocation;
    RootSignatureAllocation m_normal_allocation;
    RootSignatureAllocation m_output_allocation;
};

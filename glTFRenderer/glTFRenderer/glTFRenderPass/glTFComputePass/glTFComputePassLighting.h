#pragma once
#include "glTFComputePassBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"

class glTFComputePassLighting : public glTFComputePassBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFComputePassLighting)

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual DispatchCount GetDispatchCount(glTFRenderResourceManager& resource_manager) const override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;

    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    RootSignatureAllocation m_albedo_allocation;
    RootSignatureAllocation m_depth_allocation;
    RootSignatureAllocation m_normal_allocation;
    RootSignatureAllocation m_shadowmap_allocation;
    RootSignatureAllocation m_output_allocation;
};

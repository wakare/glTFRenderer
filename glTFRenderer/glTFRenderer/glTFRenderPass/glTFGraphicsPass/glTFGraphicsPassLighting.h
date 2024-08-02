#pragma once

#include "glTFGraphicsPassPostprocess.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceLighting.h"

class glTFGraphicsPassLighting : public glTFGraphicsPassPostprocess
{
public:
    glTFGraphicsPassLighting();

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool FinishProcessSceneObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
    std::shared_ptr<IRHITextureDescriptorAllocation> m_base_pass_albedo_allocation;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_depth_texture_allocation;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_base_pass_normal_allocation;

    RootSignatureAllocation m_albedo_allocation;
    RootSignatureAllocation m_depth_allocation;
    RootSignatureAllocation m_normal_allocation;
};

#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshOpaque : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshOpaque();
    
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
    RootSignatureAllocation m_sampler_allocation;

    std::shared_ptr<IRHITextureDescriptorAllocation> m_albedo_view;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_normal_view;
};
 
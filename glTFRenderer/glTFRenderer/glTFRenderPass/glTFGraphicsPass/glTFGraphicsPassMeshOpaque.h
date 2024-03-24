#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshOpaque : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshOpaque();
    
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    std::shared_ptr<IRHIRenderTarget> m_base_pass_color_render_target;
    std::shared_ptr<IRHIRenderTarget> m_base_pass_normal_render_target;

    RootSignatureAllocation m_sampler_allocation;
    bool m_material_uploaded;
};
 
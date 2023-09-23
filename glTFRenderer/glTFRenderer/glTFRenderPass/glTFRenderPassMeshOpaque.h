#pragma once
#include "glTFGraphicsPassMeshBase.h"
#include "../glTFMaterial/glTFMaterialParameterTexture.h"

class glTFRenderPassMeshOpaque : public glTFGraphicsPassMeshBase, public glTFRenderInterfaceSceneMeshMaterial
{
protected:
    enum glTFRenderPassMeshOpaqueRootParameterEnum
    {
        MeshOpaquePass_RootParameter_SceneMesh_SRV = MeshBasePass_RootParameter_LastIndex,
        MeshOpaquePass_RootParameter_LastIndex,
    };
    
    enum glTFRenderPassMeshBaseRegisterIndex
    {
        // Start with t0
        MeshOpaquePass_SceneMesh_SRV_Register = 0,
    };
    
public:
    glTFRenderPassMeshOpaque();
    
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool ProcessMaterial(glTFRenderResourceManager& resource_manager,const glTFMaterialBase& material) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual size_t GetRootSignatureParameterCount() override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resource_manager, glTFUniqueID meshID) override;

    std::shared_ptr<IRHIRenderTarget> m_base_pass_color_render_target;
    std::shared_ptr<IRHIRenderTarget> m_base_pass_normal_render_target;
};
 
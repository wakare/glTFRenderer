#pragma once
#include "glTFRenderPassMeshBase.h"
#include "../glTFMaterial/glTFMaterialParameterTexture.h"

class glTFRenderPassMeshOpaque : public glTFRenderPassMeshBase
{
protected:
    enum glTFRenderPassMeshOpaqueRootParameterEnum
    {
        MeshOpaquePass_RootParameter_MeshMaterialTexSRV = MeshBasePass_RootParameter_Num,
        MeshOpaquePass_RootParameter_Num,
    };
    
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    virtual bool ProcessMaterial(glTFRenderResourceManager& resourceManager,const glTFMaterialBase& material) override; 
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resourceManager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID) override;
    
    std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;
    std::vector<RHIPipelineInputLayout> ResolveVertexInputLayout(const glTFScenePrimitive& primitive);
    
    std::map<glTFUniqueID, std::unique_ptr<glTFMaterialTextureRenderResource>> m_material_texture_resources;
};
 
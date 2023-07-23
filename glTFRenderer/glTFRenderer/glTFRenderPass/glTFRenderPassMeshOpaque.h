#pragma once
#include "glTFRenderPassMeshBase.h"
#include "../glTFMaterial/glTFMaterialParameterTexture.h"

class glTFRenderPassMeshOpaque : public glTFRenderPassMeshBase
{
protected:
    enum glTFRenderPassMeshOpaqueRootParameterEnum
    {
        MeshOpaquePass_RootParameter_MeshMaterialTexSRV = MeshBasePass_RootParameter_LastIndex,
        MeshOpaquePass_RootParameter_LastIndex,
    };
    
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool InitPass(glTFRenderResourceManager& resourceManager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resourceManager) override;

    virtual bool ProcessMaterial(glTFRenderResourceManager& resource_manager,const glTFMaterialBase& material) override; 
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resourceManager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resourceManager, glTFUniqueID meshID) override;
};
 
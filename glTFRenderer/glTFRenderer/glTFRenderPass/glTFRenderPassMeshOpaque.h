#pragma once
#include "glTFRenderPassMeshBase.h"
#include "../glTFMaterial/glTFMaterialParameterTexture.h"

class glTFRenderPassMeshOpaque : public glTFRenderPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    virtual bool ProcessMaterial(glTFRenderResourceManager& resource_manager,const glTFMaterialBase& material) override; 
    
protected:
    virtual size_t GetMainDescriptorHeapSize() override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resource_manager, glTFUniqueID meshID) override;
};
 
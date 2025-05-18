#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshOpaque : public glTFGraphicsPassMeshBaseSceneView
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshOpaque)
    
    virtual const char* PassName() override {return "MeshPassOpaque"; }
    
protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
};
 
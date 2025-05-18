#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshDepth : public glTFGraphicsPassMeshBaseSceneView
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshDepth)
    
    virtual const char* PassName() override {return "MeshPassOpaqueDepth"; }
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
};

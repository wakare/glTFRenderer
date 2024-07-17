#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshDepth : public glTFGraphicsPassMeshBase
{
public:
    virtual const char* PassName() override {return "MeshPassOpaqueDepth"; }
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
};

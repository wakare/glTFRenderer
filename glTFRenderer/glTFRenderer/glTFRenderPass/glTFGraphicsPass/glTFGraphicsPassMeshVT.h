#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshVT : public glTFGraphicsPassMeshBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshVT)
    
    virtual const char* PassName() override {return "MeshPassVT"; }

protected:
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
};

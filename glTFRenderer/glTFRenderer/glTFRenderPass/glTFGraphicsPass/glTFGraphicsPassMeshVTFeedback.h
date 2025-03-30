#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshVTFeedback : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshVTFeedback(unsigned virtual_texture_id);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshVTFeedback)
    
    virtual const char* PassName() override {return "MeshPassVT"; }

protected:
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;

    unsigned m_virtual_texture_id;
};

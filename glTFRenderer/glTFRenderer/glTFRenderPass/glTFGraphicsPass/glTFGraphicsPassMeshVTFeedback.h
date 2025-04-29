#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshVTFeedback : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshVTFeedback(unsigned virtual_texture_id, bool shadowmap);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshVTFeedback)
    
    virtual const char* PassName() override {return "MeshPassVT"; }
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;

    unsigned m_virtual_texture_id;
    bool m_shadowmap;

    // Config
    int m_feedback_mipmap_offset;
};

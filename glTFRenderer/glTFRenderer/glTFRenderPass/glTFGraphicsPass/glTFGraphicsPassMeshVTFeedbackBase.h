#pragma once
#include "glTFGraphicsPassMeshBase.h"

class glTFGraphicsPassMeshVTFeedbackBase : public glTFGraphicsPassMeshBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshVTFeedbackBase)
    
    virtual const char* PassName() override {return "MeshPassVT"; }
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;

    int m_feedback_mipmap_offset {0};
};

class glTFGraphicsPassMeshVTFeedbackSVT : public glTFGraphicsPassMeshVTFeedbackBase
{
public:
    
protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
};


class glTFGraphicsPassMeshVTFeedbackRVT : public glTFGraphicsPassMeshVTFeedbackBase
{
public:
    glTFGraphicsPassMeshVTFeedbackRVT(unsigned virtual_texture_id);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshVTFeedbackRVT)
    
protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
    unsigned m_virtual_texture_id;
};

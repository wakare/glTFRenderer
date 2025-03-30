#pragma once
#include "glTFGraphicsPassMeshBase.h"

struct VSMConfig
{
    int light_id;
    unsigned virtual_texture_id;
};

class glTFGraphicsPassVirtualShadowmapFeedback : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassVirtualShadowmapFeedback(const VSMConfig& config);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassVirtualShadowmapFeedback)
    
    virtual const char* PassName() override {return "MeshPass_VirtualShadowmapFeedback"; }
    
protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitResourceTable(glTFRenderResourceManager& resource_manager) override;
    
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    VSMConfig m_config{};
};

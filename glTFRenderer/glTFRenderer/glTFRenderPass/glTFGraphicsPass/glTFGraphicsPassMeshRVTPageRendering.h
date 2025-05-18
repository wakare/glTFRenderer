#pragma once
#include "glTFGraphicsPassMeshBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

class glTFDirectionalLight;

class glTFGraphicsPassMeshRVTPageRendering : public glTFGraphicsPassMeshBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshRVTPageRendering)

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;

    virtual const char* PassName() override {return "MeshPass_RVT_Page_Rendering"; }

    // Mark next page to render
    void SetupNextPageRenderingInfo(glTFRenderResourceManager& resource_manager, const RVTPageRenderingInfo& page_rendering_info);

protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    RVTPageRenderingInfo m_page_rendering_info;
};

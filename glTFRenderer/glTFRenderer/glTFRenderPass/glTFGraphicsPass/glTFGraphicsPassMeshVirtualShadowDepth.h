#pragma once
#include "glTFGraphicsPassMeshBase.h"
#include "glTFGraphicsPassMeshShadowDepth.h"

struct VSMConfig
{
    ShadowmapPassConfig m_shadowmap_config{};
    unsigned virtual_texture_id;
};

struct VSMPageRenderingInfo
{
    float mip_page_size;
    float page_x;
    float page_y;
    
    int physical_page_size;
    int physical_page_x;
    int physical_page_y;
};

class glTFGraphicsPassMeshVirtualShadowDepth : public glTFGraphicsPassMeshBase
{
public:
    glTFGraphicsPassMeshVirtualShadowDepth(const VSMConfig& config);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshVirtualShadowDepth)

    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;

    virtual const char* PassName() override {return "MeshPass_VirtualShadowmap"; }

    // Mark next page to render
    void SetupNextPageRenderingInfo(const VSMPageRenderingInfo& page_rendering_info);
    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;

protected:
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PostRenderPass(glTFRenderResourceManager& resource_manager) override;
    
    virtual RHIViewportDesc GetViewport(glTFRenderResourceManager& resource_manager) const override;

    VSMConfig m_config;
    std::shared_ptr<IRHITexture> m_rvt_output_texture;
    RootSignatureAllocation m_rvt_output_allocation;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_rvt_descriptor_allocations;

    VSMPageRenderingInfo m_page_rendering_info;
};

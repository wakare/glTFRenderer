#pragma once
#include "glTFGraphicsPassMeshRVTPageRendering.h"
#include "glTFGraphicsPassMeshShadowDepth.h"

struct VSMConfig
{
    ShadowmapPassConfig m_shadowmap_config{};
    unsigned virtual_texture_id;
};

class glTFGraphicsPassMeshVSMPageRendering : public glTFGraphicsPassMeshRVTPageRendering
{
public:
    glTFGraphicsPassMeshVSMPageRendering(const VSMConfig& config);
    DECLARE_NON_COPYABLE_AND_VDTOR(glTFGraphicsPassMeshVSMPageRendering)

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    
protected:
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;

    VSMConfig m_config;
    
    std::shared_ptr<IRHITexture> m_rvt_output_texture;
    RootSignatureAllocation m_rvt_output_allocation;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_rvt_descriptor_allocations;

    const glTFDirectionalLight* m_directional_light{nullptr};
};

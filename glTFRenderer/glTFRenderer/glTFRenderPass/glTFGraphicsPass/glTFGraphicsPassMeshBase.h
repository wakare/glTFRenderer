#pragma once

#include "glTFGraphicsPassBase.h"

class RHIVertexStreamingManager;
class IRHICommandSignature;

// Drawing all meshes within mesh pass
class glTFGraphicsPassMeshBase : public glTFGraphicsPassBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshBase)
    
    virtual const char* PassName() override {return "MeshPass"; }
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    bool InitPass(glTFRenderResourceManager& resource_manager) override;
    bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    bool RenderPass(glTFRenderResourceManager& resource_manager) override;

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resource_manager, const glTFSceneObjectBase& object) override;
    virtual bool UpdateGUIWidgets() override;
    
protected:
    virtual RHICullMode GetCullMode() override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool UsingIndirectDraw() const;
    virtual bool UsingIndirectDrawCulling() const;
    virtual bool UsingInputLayout() const;

    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const override;
    
    bool m_indirect_draw {true};
    
    // Indirect drawing
    std::shared_ptr<IRHICommandSignature> m_command_signature;
};

class glTFGraphicsPassMeshBaseSceneView : public glTFGraphicsPassMeshBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassMeshBaseSceneView)
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool UpdateGUIWidgets() override;
    virtual bool UsingIndirectDrawCulling() const override;
    
protected:
    bool m_use_indirect_culling {true};
};

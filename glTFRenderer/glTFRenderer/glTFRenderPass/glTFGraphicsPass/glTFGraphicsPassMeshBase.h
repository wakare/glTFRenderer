#pragma once

#include "glTFGraphicsPassBase.h"

class RHIVertexStreamingManager;
class IRHICommandSignature;

// Drawing all meshes within mesh pass
class glTFGraphicsPassMeshBase : public glTFGraphicsPassBase
{
public:
    glTFGraphicsPassMeshBase();
    virtual const char* PassName() override {return "MeshPass"; }
    
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

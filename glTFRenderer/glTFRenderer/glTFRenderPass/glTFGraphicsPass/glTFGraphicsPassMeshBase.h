#pragma once

#include "glTFGraphicsPassBase.h"
#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

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

protected:
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout(glTFRenderResourceManager& resource_manager) override;
    
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned instance_offset);
    virtual bool EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index);
    
    virtual bool UsingIndirectDraw() const { return true; }
    virtual bool UsingIndirectDrawCulling() const { return false;}
    virtual bool UsingInputLayout() const {return true; }
    
protected:
    // Indirect drawing
    std::shared_ptr<IRHICommandSignature> m_command_signature;
};

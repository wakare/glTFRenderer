#pragma once

#include "glTFGraphicsPassBase.h"
#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMesh.h"
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

    virtual bool TryProcessSceneObject(glTFRenderResourceManager& resourceManager, const glTFSceneObjectBase& object) override;

    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);
    
protected:
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

    virtual bool BeginDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index);
    virtual bool EndDrawMesh(glTFRenderResourceManager& resourceManager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index);

    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;

    std::shared_ptr<IRHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;
    
};

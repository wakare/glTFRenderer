#pragma once

#include "glTFGraphicsPassBase.h"
#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

struct MeshInstanceInputData
{
    glm::mat4 instance_transform;
    unsigned instance_material_id;
    unsigned normal_mapping;
    unsigned padding[2];
};

struct MeshInstanceInputLayout
{
    VertexLayoutDeclaration m_instance_layout;
    
    MeshInstanceInputLayout()
    {
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_0, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_1, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_2, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_MAT_3, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_CUSTOM_DATA, sizeof(unsigned) * 4});
    }

    bool ResolveInputInstanceLayout(std::vector<RHIPipelineInputLayout>& out_layout)
    {
        unsigned vertex_layout_offset = 0;
        for (const auto& vertex_layout : m_instance_layout.elements)
        {
            switch (vertex_layout.type)
            {
            case VertexLayoutType::INSTANCE_MAT_0:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexLayoutType::INSTANCE_MAT_1:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 1, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexLayoutType::INSTANCE_MAT_2:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 2, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexLayoutType::INSTANCE_MAT_3:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 3, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexLayoutType::INSTANCE_CUSTOM_DATA:
                {
                    out_layout.push_back({ "INSTANCE_CUSTOM_DATA", 0, RHIDataFormat::R32G32B32A32_UINT, vertex_layout_offset, 1 });
                }
                break;
            default:
                return false;
            }

            vertex_layout_offset += vertex_layout.byte_size;   
        }

        return true;
    }
};

struct MeshIndirectDrawCommand
{
    MeshIndirectDrawCommand(const IRHIVertexBufferView& vertex_buffer_view, const IRHIVertexBufferView& instance_buffer_view, const IRHIIndexBufferView& index_buffer_view)
        : vertex_buffer_view(vertex_buffer_view)
        , vertex_buffer_instance_view(instance_buffer_view)
        , index_buffer_view(index_buffer_view)
    {
        
    }
    
    // VB for mesh
    RHIIndirectArgumentVertexBufferView vertex_buffer_view;

    // VB for instancing
    RHIIndirectArgumentVertexBufferView vertex_buffer_instance_view;

    // IB
    RHIIndirectArgumentIndexBufferView index_buffer_view;
    
    // Draw arguments
    RHIIndirectArgumentDrawIndexed draw_command_argument;
};

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
    virtual bool EndDrawMesh(glTFRenderResourceManager& resource_manager, const glTFMeshRenderResource& mesh_data, unsigned mesh_index);

    bool InitInstanceBuffer(glTFRenderResourceManager& resource_manager);
    
    virtual std::vector<RHIPipelineInputLayout> GetVertexInputLayout() override;

    virtual bool UsingIndirectDraw() const { return true; }
    
protected:
    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;
    MeshInstanceInputLayout m_instance_input_layout;

    // mesh id -- <instance count, instance start offset>
    std::map<glTFUniqueID, std::pair<unsigned, unsigned>> m_instance_draw_infos;
    
    std::shared_ptr<IRHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;
    
    // Indirect drawing
    std::shared_ptr<IRHICommandSignature> m_command_signature;

    std::vector<MeshIndirectDrawCommand> m_indirect_arguments;
    std::shared_ptr<IRHIGPUBuffer> m_indirect_argument_buffer;
};

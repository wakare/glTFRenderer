#pragma once
#include "glTFMeshRenderResource.h"
#include "glTFRHI/RHIInterface/IRHICommandSignature.h"
#include "glTFRHI/RHIInterface/IRHIMemoryManager.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

class glTFRenderResourceManager;

struct glTFMeshInstanceRenderResource
{
    glm::mat4 m_instance_transform;
    glTFUniqueID m_mesh_render_resource;
    unsigned m_instance_material_id;
    bool m_normal_mapping;
};

ALIGN_FOR_CBV_STRUCT struct MeshIndirectDrawCommand
{
    inline static std::string Name = "INDIRECT_DRAW_DATA_REGISTER_SRV_INDEX";
    
    MeshIndirectDrawCommand(
        const IRHIVertexBufferView& vertex_buffer_view,
        const IRHIVertexBufferView& instance_buffer_view,
        const IRHIIndexBufferView& index_buffer_view
        )
        : vertex_buffer_view(vertex_buffer_view)
        , vertex_buffer_instance_view(instance_buffer_view)
        , index_buffer_view(index_buffer_view)
        , draw_command_argument({0, 0, 0, 0, 0})
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


ALIGN_FOR_CBV_STRUCT struct MeshInstanceInputData
{
    inline static std::string Name = "MESH_INSTANCE_INPUT_DATA_REGISTER_SRV_INDEX";
    
    glm::mat4 instance_transform;
    unsigned instance_material_id;
    unsigned normal_mapping;
    unsigned mesh_id;
    unsigned padding;
};

struct MeshInstanceInputLayout
{
    VertexLayoutDeclaration m_instance_layout;
    
    MeshInstanceInputLayout()
    {
        m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_0, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_1, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_2, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_3, sizeof(float) * 4});
        m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_CUSTOM_DATA, sizeof(unsigned) * 4});
    }

    bool ResolveInputInstanceLayout(std::vector<RHIPipelineInputLayout>& out_layout)
    {
        unsigned vertex_layout_offset = 0;
        for (const auto& vertex_layout : m_instance_layout.elements)
        {
            switch (vertex_layout.type)
            {
            case VertexAttributeType::INSTANCE_MAT_0:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexAttributeType::INSTANCE_MAT_1:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 1, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexAttributeType::INSTANCE_MAT_2:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 2, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexAttributeType::INSTANCE_MAT_3:
                {
                    out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 3, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1 });
                }
                break;
            case VertexAttributeType::INSTANCE_CUSTOM_DATA:
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

class glTFRenderMeshManager
{
public:
    glTFRenderMeshManager();
    
    bool AddOrUpdatePrimitive(glTFRenderResourceManager& resource_manager, const glTFScenePrimitive* primitive);
    bool BuildMeshRenderResource(glTFRenderResourceManager& resource_manager);

    const std::map<glTFUniqueID, glTFMeshInstanceRenderResource>& GetMeshInstanceRenderResource() const {return m_mesh_instances; }
    const std::map<glTFUniqueID, glTFMeshRenderResource>& GetMeshRenderResources() const {return m_mesh_render_resources; }

    const RHICommandSignatureDesc& GetCommandSignatureDesc() const {return m_command_signature_desc; }
    const std::vector<RHIPipelineInputLayout>& GetVertexInputLayout() const {return m_vertex_input_layouts; }
    MeshInstanceInputLayout GetInstanceInputLayout() const {return m_instance_input_layout; }

    const std::vector<MeshIndirectDrawCommand>& GetIndirectDrawCommands() const {return m_indirect_arguments; }
    std::shared_ptr<IRHIBuffer> GetIndirectArgumentBuffer() const {return m_indirect_argument_buffer->m_buffer; }
    std::shared_ptr<IRHIBuffer> GetCulledIndirectArgumentBuffer() const {return m_culled_indirect_commands->m_buffer; }
    unsigned GetCulledIndirectArgumentBufferCountOffset() const {return m_culled_indirect_command_count_offset; }

    const std::vector<MeshInstanceInputData>& GetInstanceBufferData() const {return m_instance_data; }
    std::shared_ptr<IRHIVertexBuffer> GetInstanceBuffer() const {return m_instance_buffer; }
    std::shared_ptr<IRHIVertexBufferView> GetInstanceBufferView() const {return m_instance_buffer_view; }
    const std::map<glTFUniqueID, std::pair<unsigned, unsigned>>& GetInstanceDrawInfo() const {return m_instance_draw_infos; }
    
    bool ResolveVertexInputLayout(const VertexLayoutDeclaration& source_vertex_layout);
    
protected:
    std::map<glTFUniqueID, glTFMeshInstanceRenderResource> m_mesh_instances;
    std::map<glTFUniqueID, glTFMeshRenderResource> m_mesh_render_resources;

    RHICommandSignatureDesc m_command_signature_desc;
    
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;
    MeshInstanceInputLayout m_instance_input_layout;

    std::vector<MeshIndirectDrawCommand> m_indirect_arguments;
    std::shared_ptr<IRHIBufferAllocation> m_indirect_argument_buffer;

    std::shared_ptr<IRHIBufferAllocation> m_culled_indirect_commands;
    unsigned m_culled_indirect_command_count_offset;
    
    // mesh id -- <instance count, instance start offset>
    std::map<glTFUniqueID, std::pair<unsigned, unsigned>> m_instance_draw_infos;

    std::vector<MeshInstanceInputData> m_instance_data;
    std::shared_ptr<IRHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;
};

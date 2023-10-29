#pragma once

#include "glTFGraphicsPassBase.h"
#include "glTFScene/glTFScenePrimitive.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"

struct MeshInstanceInputData
{
    glm::mat4 instance_transform;
    unsigned instance_material_id;
    unsigned padding[3];
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
        m_instance_layout.elements.push_back({VertexLayoutType::INSTANCE_MATERIAL_ID, sizeof(float) * 4});
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
            case VertexLayoutType::INSTANCE_MATERIAL_ID:
                {
                    out_layout.push_back({ "INSTANCE_MATERIAL_ID", 0, RHIDataFormat::R32_UINT, vertex_layout_offset, 1 });
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

    // TODO: Resolve input layout with multiple meshes 
    std::vector<RHIPipelineInputLayout> m_vertex_input_layouts;
    MeshInstanceInputLayout m_instance_input_layout;

    // mesh id -- <instance count, instance start offset>
    std::map<glTFUniqueID, std::pair<unsigned, unsigned>> m_instance_draw_infos;
    
    std::shared_ptr<IRHIVertexBuffer> m_instance_buffer;
    std::shared_ptr<IRHIVertexBufferView> m_instance_buffer_view;
    
};

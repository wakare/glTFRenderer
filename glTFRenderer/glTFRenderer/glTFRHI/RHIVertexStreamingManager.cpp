#include "RHIVertexStreamingManager.h"

#include "RHIConfigSingleton.h"
#include "RHIInterface/IRHIPipelineStateObject.h"

MeshVertexInputLayout::MeshVertexInputLayout()
{
    m_source_vertex_layout.elements.push_back({VertexAttributeType::VERTEX_POSITION, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)});
    m_source_vertex_layout.elements.push_back({VertexAttributeType::VERTEX_NORMAL, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)});
    m_source_vertex_layout.elements.push_back({VertexAttributeType::VERTEX_TANGENT, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)});
    m_source_vertex_layout.elements.push_back({VertexAttributeType::VERTEX_TEXCOORD0, GetBytePerPixelByFormat(RHIDataFormat::R32G32_FLOAT)});
}

bool MeshVertexInputLayout::ResolveInputVertexLayout(std::vector<RHIPipelineInputLayout>& m_vertex_input_layouts) const
{
    unsigned vertex_layout_offset = 0;
    unsigned vertex_layout_location = 0;
        
    for (const auto& vertex_layout : m_source_vertex_layout.elements)
    {
        bool added = false;
        switch (vertex_layout.type)
        {
        case VertexAttributeType::VERTEX_POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_TEXCOORD0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32_FLOAT)));
                m_vertex_input_layouts.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
            // TODO: Handle TEXCOORD_1?
        }
            
        if (added)
        {
            ++vertex_layout_location;
            vertex_layout_offset += vertex_layout.byte_size;    
        }
    }
        
    return true;
}

MeshInstanceInputLayout::MeshInstanceInputLayout()
{
    m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_0, sizeof(float) * 4});
    m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_1, sizeof(float) * 4});
    m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_2, sizeof(float) * 4});
    m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_MAT_3, sizeof(float) * 4});
    m_instance_layout.elements.push_back({VertexAttributeType::INSTANCE_CUSTOM_DATA, sizeof(unsigned) * 4});
}

bool MeshInstanceInputLayout::ResolveInputInstanceLayout(std::vector<RHIPipelineInputLayout>& out_layout) const
{
    unsigned vertex_layout_offset = 0;
    unsigned vertex_layout_location = 0;
    for (const auto& vertex_layout : m_instance_layout.elements)
    {
        switch (vertex_layout.type)
        {
        case VertexAttributeType::INSTANCE_MAT_0:
            {
                out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location });
            }
            break;
        case VertexAttributeType::INSTANCE_MAT_1:
            {
                out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 1, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location });
            }
            break;
        case VertexAttributeType::INSTANCE_MAT_2:
            {
                out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 2, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location });
            }
            break;
        case VertexAttributeType::INSTANCE_MAT_3:
            {
                out_layout.push_back({ "INSTANCE_TRANSFORM_MATRIX", 3, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location });
            }
            break;
        case VertexAttributeType::INSTANCE_CUSTOM_DATA:
            {
                out_layout.push_back({ "INSTANCE_CUSTOM_DATA", 0, RHIDataFormat::R32G32B32A32_UINT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location });
            }
            break;
        default:
            return false;
        }
        ++vertex_layout_location;
        vertex_layout_offset += vertex_layout.byte_size;   
    }

    return true;
}


bool RHIVertexStreamingManager::Init(const VertexLayoutDeclaration& vertex_layout, bool has_instance)
{
    m_vertex_input_layout.m_source_vertex_layout = vertex_layout;
    GLTF_CHECK(m_vertex_input_layout.ResolveInputVertexLayout(m_vertex_attributes));
    if (has_instance)
    {
        GLTF_CHECK(m_instance_input_layout.ResolveInputInstanceLayout(m_vertex_attributes));    
    }
    
    return true;
}

void RHIVertexStreamingManager::ConfigShaderMacros(RHIShaderPreDefineMacros& shader_macros) const
{
    // Add shader pre define macros
    unsigned attribute_location = 0;
    for (const auto& input_layout : m_vertex_attributes)
    {
        if (input_layout.semantic_name == INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL))
        {
            shader_macros.AddMacro("HAS_NORMAL", "1");
        }

        if (input_layout.semantic_name == INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD))
        {
            shader_macros.AddMacro("HAS_TEXCOORD", "1");
        }

        if (input_layout.semantic_name == INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT))
        {
            shader_macros.AddMacro("HAS_TANGENT", "1");
        }

        if (RHIConfigSingleton::Instance().GetGraphicsAPIType() == RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan)
        {
            char vulkan_attribute_location[64] = {'\0'};
            (void)snprintf(vulkan_attribute_location, sizeof(vulkan_attribute_location), "[[vk::location(%d)]]", attribute_location);
            shader_macros.AddMacro(input_layout.semantic_name + "_VK", vulkan_attribute_location);
        }

        attribute_location++;
    }
}

const MeshInstanceInputLayout& RHIVertexStreamingManager::GetInstanceInputLayout() const
{
    return m_instance_input_layout;
}

const std::vector<RHIPipelineInputLayout>& RHIVertexStreamingManager::GetVertexAttributes() const
{
    return m_vertex_attributes;
}

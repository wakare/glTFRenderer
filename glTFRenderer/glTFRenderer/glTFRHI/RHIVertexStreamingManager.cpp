#include "RHIVertexStreamingManager.h"

#include "glTFRHI/RHIConfigSingleton.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"

bool RHIVertexStreamingManager::Init(const VertexLayoutDeclaration& vertex_layout_declaration, bool has_instance)
{
    unsigned vertex_layout_offset = 0;
    unsigned vertex_layout_location = 0;
        
    for (const auto& vertex_layout : vertex_layout_declaration.elements)
    {
        bool added = false;
        switch (vertex_layout.type)
        {
        case VertexAttributeType::VERTEX_POSITION:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)));
                m_vertex_attributes.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(POSITION), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_NORMAL:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT)));
                m_vertex_attributes.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(NORMAL), 0, RHIDataFormat::R32G32B32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_TANGENT:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)));
                m_vertex_attributes.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TANGENT), 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        case VertexAttributeType::VERTEX_TEXCOORD0:
            {
                GLTF_CHECK(vertex_layout.byte_size == (GetBytePerPixelByFormat(RHIDataFormat::R32G32_FLOAT)));
                m_vertex_attributes.push_back({INPUT_LAYOUT_UNIQUE_PARAMETER(TEXCOORD), 0, RHIDataFormat::R32G32_FLOAT, vertex_layout_offset, 0, PER_VERTEX, vertex_layout_location});
                added = true;
            }
            break;
        }
            
        if (added)
        {
            ++vertex_layout_location;
            vertex_layout_offset += vertex_layout.byte_size;    
        }
    }
    
    if (has_instance)
    {
        vertex_layout_offset = 0;
        
        m_vertex_attributes.push_back({ "INSTANCE_TRANSFORM_MATRIX", 0, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location++ });
        vertex_layout_offset += GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT);
        m_vertex_attributes.push_back({ "INSTANCE_TRANSFORM_MATRIX", 1, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location++ });
        vertex_layout_offset += GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT);
        m_vertex_attributes.push_back({ "INSTANCE_TRANSFORM_MATRIX", 2, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location++ });
        vertex_layout_offset += GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT);
        m_vertex_attributes.push_back({ "INSTANCE_TRANSFORM_MATRIX", 3, RHIDataFormat::R32G32B32A32_FLOAT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location++ });
        vertex_layout_offset += GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT);
        m_vertex_attributes.push_back({ "INSTANCE_CUSTOM_DATA", 0, RHIDataFormat::R32G32B32A32_UINT, vertex_layout_offset, 1, PER_INSTANCE, vertex_layout_location++ });
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
            shader_macros.AddMacro(input_layout.semantic_name + std::to_string(input_layout.semantic_index) + "_VK", vulkan_attribute_location);
        }

        attribute_location++;
    }
}

std::vector<VertexAttributeElement> RHIVertexStreamingManager::GetInstanceInputLayout() const
{
    std::vector<VertexAttributeElement> results;
    results.push_back({VertexAttributeType::INSTANCE_MAT_0, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)});
    results.push_back({VertexAttributeType::INSTANCE_MAT_1, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)});
    results.push_back({VertexAttributeType::INSTANCE_MAT_2, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)});
    results.push_back({VertexAttributeType::INSTANCE_MAT_3, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_FLOAT)});
    results.push_back({VertexAttributeType::INSTANCE_CUSTOM_DATA, GetBytePerPixelByFormat(RHIDataFormat::R32G32B32A32_UINT)});
    return results;
}

const std::vector<RHIPipelineInputLayout>& RHIVertexStreamingManager::GetVertexAttributes() const
{
    return m_vertex_attributes;
}

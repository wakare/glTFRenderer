#include "glTFGraphicsPassTestIndexedTriangle.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

struct Vertex
{
    float position[3];
    float uv[2];
};

Vertex vertices[] =
{
    Vertex({-0.5f, -0.5f,  0.0f}, {0.0f, 0.0f}),
    Vertex({0.5f,  -0.5f,  0.0f}, {1.0f, 0.0f}),
    Vertex({0.0f,  0.5f,  0.0f}, {0.0f, 1.0f}),
};

unsigned indices[] =
{
    0, 1, 2
};

const char* glTFGraphicsPassTestIndexedTriangle::PassName()
{
    return "GraphicsPass_TestIndexedTriangle";
}

bool glTFGraphicsPassTestIndexedTriangle::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::InitPass(resource_manager))

    bool inited = InitVertexBufferAndIndexBuffer(resource_manager);
    GLTF_CHECK(inited);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTriangle::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetVertexBufferView(command_list, 0, *m_vertex_buffer_view);
    RHIUtils::Instance().SetIndexBufferView(command_list, *m_index_buffer_view);

    RHIUtils::Instance().DrawIndexInstanced(command_list, 3, 1, 0,0, 0);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTriangle::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTriangleVertStreamIn.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTriangleFrag.hlsl)", RHIShaderType::Pixel, "main");
    
    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);

    RHIPipelineInputLayout position_input_layout{};
    position_input_layout.format = RHIDataFormat::R32G32B32_FLOAT;
    position_input_layout.slot = 0;
    position_input_layout.semantic_index = 0;
    position_input_layout.semantic_name = "POSITION";
    position_input_layout.aligned_byte_offset = 0;
    position_input_layout.frequency = PER_VERTEX;
    position_input_layout.layout_location = 0;

    RHIPipelineInputLayout uv_input_layout{};
    uv_input_layout.format = RHIDataFormat::R32G32_FLOAT;
    uv_input_layout.slot = 0;
    uv_input_layout.semantic_index = 0;
    uv_input_layout.semantic_name = "TEXCOORD";
    uv_input_layout.aligned_byte_offset = 12;
    uv_input_layout.frequency = PER_VERTEX;
    uv_input_layout.layout_location = 1;
    
    // Set shader macro based vertex attributes
    RETURN_IF_FALSE(GetGraphicsPipelineStateObject().BindInputLayoutAndSetShaderMacros({position_input_layout, uv_input_layout}));
    
    return true;
}

bool glTFGraphicsPassTestIndexedTriangle::InitVertexBufferAndIndexBuffer(glTFRenderResourceManager& resource_manager)
{
    m_vertex_buffer_data = std::make_shared<VertexBufferData>();
    unsigned vertex_count = sizeof(vertices) / sizeof(Vertex);
    unsigned long long vertex_buffer_data_size = sizeof(Vertex) * vertex_count;
    
    m_vertex_buffer_data->data = std::make_unique<char[]>(vertex_buffer_data_size);
    m_vertex_buffer_data->byte_size = vertex_buffer_data_size;
    m_vertex_buffer_data->layout.elements.push_back({VertexAttributeType::POSITION, 12});
    m_vertex_buffer_data->layout.elements.push_back({VertexAttributeType::TEXCOORD_0, 8});
    m_vertex_buffer_data->vertex_count = vertex_count;
    memcpy(m_vertex_buffer_data->data.get(), vertices, sizeof(vertices));
    
    m_vertex_buffer = RHIResourceFactory::CreateRHIResource<IRHIVertexBuffer>();
    RHIBufferDesc vertex_buffer_desc{};
    vertex_buffer_desc.name = L"TestIndexedTriangle_vertex_buffer";
    vertex_buffer_desc.type = RHIBufferType::Default;
    vertex_buffer_desc.width = vertex_buffer_data_size;
    vertex_buffer_desc.height = 1;
    vertex_buffer_desc.depth = 1;
    vertex_buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_VERTEX_BUFFER | RUF_TRANSFER_DST);
    
    m_vertex_buffer_view = m_vertex_buffer->CreateVertexBufferView(
        resource_manager.GetDevice(),
        resource_manager.GetMemoryManager(),
        resource_manager.GetCommandListForRecord(),
        vertex_buffer_desc,
        *m_vertex_buffer_data);

    m_index_buffer_data = std::make_shared<IndexBufferData>();
    unsigned index_count = sizeof (indices) / sizeof(unsigned);
    size_t index_buffer_data_size = index_count * sizeof(unsigned);
    m_index_buffer_data->data = std::make_unique<char[]>(index_buffer_data_size);
    m_index_buffer_data->format = RHIDataFormat::R32_UINT;
    m_index_buffer_data->index_count = index_count;
    m_index_buffer_data->byte_size = index_buffer_data_size;
    memcpy(m_index_buffer_data->data.get(), indices, sizeof(indices));
    
    RHIBufferDesc index_buffer_desc{};
    index_buffer_desc.name = L"TestIndexedTriangle_index_buffer";
    index_buffer_desc.type = RHIBufferType::Default;
    index_buffer_desc.width = index_buffer_data_size;
    index_buffer_desc.height = 1;
    index_buffer_desc.depth = 1;
    index_buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST);
    
    m_index_buffer = RHIResourceFactory::CreateRHIResource<IRHIIndexBuffer>();
    m_index_buffer_view = m_index_buffer->CreateIndexBufferView(
        resource_manager.GetDevice(),
        resource_manager.GetMemoryManager(), 
        resource_manager.GetCommandListForRecord(),
        index_buffer_desc,
        *m_index_buffer_data);
    
    return true;
}

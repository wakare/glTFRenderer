#include "glTFGraphicsPassTestIndexedTextureTriangle.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

struct Vertex
{
    float position[3];
    float uv[2];
};

Vertex textured_vertices[] =
{
    Vertex({-0.5f, -0.5f,  0.0f}, {0.0f, 0.0f}),
    Vertex({0.5f,  -0.5f,  0.0f}, {1.0f, 0.0f}),
    Vertex({0.0f,  0.5f,  0.0f}, {0.0f, 1.0f}),
};

unsigned textured_indices[] =
{
    0, 1, 2
};

const char* glTFGraphicsPassTestIndexedTextureTriangle::PassName()
{
    return "GraphicsPass_TestIndexedTextureTriangle";
}

bool glTFGraphicsPassTestIndexedTextureTriangle::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::InitRenderInterface(resource_manager))
    
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
            std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::InitPass(resource_manager))

    bool inited = InitVertexBufferAndIndexBuffer(resource_manager);
    GLTF_CHECK(inited);

    // Init sampled texture content
    RHITextureDesc sampled_texture_desc =
        RHITextureDesc::MakeFullScreenTextureDesc(
            "sampled_texture",
            RHIDataFormat::R32G32B32A32_FLOAT,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST),
            RHITextureClearValue{},
            resource_manager);

    size_t texture_data_byte_size = sampled_texture_desc.GetTextureWidth() * sampled_texture_desc.GetTextureHeight() * GetBytePerPixelByFormat(sampled_texture_desc.GetDataFormat());
    std::unique_ptr<char[]> texture_data = std::make_unique<char[]>(texture_data_byte_size);
    
    for (unsigned w = 0; w < sampled_texture_desc.GetTextureWidth(); ++w)
    {
        for (unsigned h = 0; h < sampled_texture_desc.GetTextureHeight(); ++h)
        {
            float* texture_pixel_data = reinterpret_cast<float*>(texture_data.get() + GetBytePerPixelByFormat(sampled_texture_desc.GetDataFormat()) * (
                sampled_texture_desc.GetTextureWidth() * h + w));

            // Fill out RGBA channel
            texture_pixel_data[0] = static_cast<float>(w) / static_cast<float>(sampled_texture_desc.GetTextureWidth());
            texture_pixel_data[1] = static_cast<float>(h) / static_cast<float>(sampled_texture_desc.GetTextureHeight());
            texture_pixel_data[2] = 0.5f;
            texture_pixel_data[3] = 1.0f;
        }
    }
    
    sampled_texture_desc.SetTextureData(texture_data.get(), texture_data_byte_size);
    resource_manager.GetMemoryManager().AllocateTextureMemoryAndUpload(
        resource_manager.GetDevice(),
        resource_manager,
        resource_manager.GetCommandListForRecord(),
        sampled_texture_desc,
        m_sampled_texture);
    
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_sampled_texture->m_texture,
                        {m_sampled_texture->m_texture->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_sampled_texture_allocation))

    m_sampled_texture->m_texture->Transition(resource_manager.GetCommandListForRecord(), RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();

    m_sampled_texture->m_texture->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
    BindDescriptor(command_list, m_sampled_texture_root_signature_allocation, *m_sampled_texture_allocation);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::RenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RHIUtils::Instance().SetVertexBufferView(command_list, 0, *m_vertex_buffer_view);
    RHIUtils::Instance().SetIndexBufferView(command_list, *m_index_buffer_view);

    RHIUtils::Instance().DrawIndexInstanced(command_list, 3, 1, 0,0, 0);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::SetupRootSignature(resource_manager))

    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SAMPLED_TEX_REGISTER_INDEX", {RHIRootParameterDescriptorRangeType::SRV, 1, false, false}, m_sampled_texture_root_signature_allocation))

    VertexAttributeElement position_attribute;
    position_attribute.type = VertexAttributeType::VERTEX_POSITION;
    position_attribute.byte_size = GetBytePerPixelByFormat(RHIDataFormat::R32G32B32_FLOAT);
    VertexAttributeElement uv_attribute;
    uv_attribute.type = VertexAttributeType::VERTEX_TEXCOORD0;
    uv_attribute.byte_size = GetBytePerPixelByFormat(RHIDataFormat::R32G32_FLOAT);
    
    VertexLayoutDeclaration vertex_layout_declaration{};
    vertex_layout_declaration.elements.push_back(position_attribute);
    vertex_layout_declaration.elements.push_back(uv_attribute);
    
    m_vertex_streaming_manager.Init(vertex_layout_declaration, false);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassTestTriangleBase::SetupPipelineStateObject(resource_manager))

    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTexturedTriangleVert.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestTexturedTriangleFrag.hlsl)", RHIShaderType::Pixel, "main");
    
    std::vector<IRHIDescriptorAllocation*> render_targets;
    render_targets.push_back(&resource_manager.GetCurrentFrameSwapChainRTV());
    GetGraphicsPipelineStateObject().BindRenderTargetFormats(render_targets);

    auto& shaderMacros = GetGraphicsPipelineStateObject().GetShaderMacros();
    m_sampled_texture_root_signature_allocation.AddShaderDefine(shaderMacros);
    
    return true;
}

bool glTFGraphicsPassTestIndexedTextureTriangle::InitVertexBufferAndIndexBuffer(
    glTFRenderResourceManager& resource_manager)
{
    m_vertex_buffer_data = std::make_shared<VertexBufferData>();
    unsigned vertex_count = sizeof(textured_vertices) / sizeof(Vertex);
    unsigned long long vertex_buffer_data_size = sizeof(Vertex) * vertex_count;
    
    m_vertex_buffer_data->data = std::make_unique<char[]>(vertex_buffer_data_size);
    m_vertex_buffer_data->byte_size = vertex_buffer_data_size;
    m_vertex_buffer_data->layout.elements.push_back({VertexAttributeType::VERTEX_POSITION, 12});
    m_vertex_buffer_data->layout.elements.push_back({VertexAttributeType::VERTEX_TEXCOORD0, 8});
    m_vertex_buffer_data->vertex_count = vertex_count;
    memcpy(m_vertex_buffer_data->data.get(), textured_vertices, sizeof(textured_vertices));
    
    m_vertex_buffer = std::make_shared<RHIVertexBuffer>();
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
    unsigned index_count = sizeof (textured_indices) / sizeof(unsigned);
    size_t index_buffer_data_size = index_count * sizeof(unsigned);
    m_index_buffer_data->data = std::make_unique<char[]>(index_buffer_data_size);
    m_index_buffer_data->format = RHIDataFormat::R32_UINT;
    m_index_buffer_data->index_count = index_count;
    m_index_buffer_data->byte_size = index_buffer_data_size;
    memcpy(m_index_buffer_data->data.get(), textured_indices, sizeof(textured_indices));
    
    RHIBufferDesc index_buffer_desc{};
    index_buffer_desc.name = L"TestIndexedTriangle_index_buffer";
    index_buffer_desc.type = RHIBufferType::Default;
    index_buffer_desc.width = index_buffer_data_size;
    index_buffer_desc.height = 1;
    index_buffer_desc.depth = 1;
    index_buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST);
    
    m_index_buffer = std::make_shared<RHIIndexBuffer>();
    m_index_buffer_view = m_index_buffer->CreateIndexBufferView(
        resource_manager.GetDevice(),
        resource_manager.GetMemoryManager(), 
        resource_manager.GetCommandListForRecord(),
        index_buffer_desc,
        *m_index_buffer_data);
    
    return true;
}

const RHIVertexStreamingManager& glTFGraphicsPassTestIndexedTextureTriangle::GetVertexStreamingManager(
    glTFRenderResourceManager& resource_manager) const
{
    return m_vertex_streaming_manager;
}

#include "ResourceManager.h"

#include "InternalResourceHandleTable.h"
#include "RenderPass.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandList.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHIRootSignatureHelper.h"
#include "RHIInterface/IRHISwapChain.h"
#include "RHIInterface/RHIIndexBuffer.h"

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

RHIDataFormat RendererInterfaceRHIConverter::ConvertToRHIFormat(RendererInterface::PixelFormat format)
{
    switch (format) {
    case RendererInterface::RGBA8_UNORM:
        return RHIDataFormat::R8G8B8A8_UNORM;
    
    case RendererInterface::RGBA16_UNORM:
        return RHIDataFormat::R16G16B16A16_UNORM;
    
    }
    
    GLTF_CHECK(false);
    return RHIDataFormat::UNKNOWN;
}

RHIPipelineType RendererInterfaceRHIConverter::ConvertToRHIPipelineType(RendererInterface::RenderPassType type)
{
    switch (type) {
    case RendererInterface::GRAPHICS:
        return RHIPipelineType::Graphics;
        
    case RendererInterface::COMPUTE:
        return RHIPipelineType::Compute;
        
    case RendererInterface::RAY_TRACING:
        return RHIPipelineType::RayTracing;
        
    }
    GLTF_CHECK(false);
    return RHIPipelineType::Unknown;
}

RHIBufferDesc ConvertToRHIBufferDesc(const RendererInterface::BufferDesc& desc)
{
    RHIBufferDesc buffer_desc{};
    buffer_desc.name = to_wide_string(desc.name);
    buffer_desc.width = desc.size;
    buffer_desc.height = 1;
    buffer_desc.depth = 1;
    
    switch (desc.type)
    {
    case RendererInterface::UPLOAD:
        buffer_desc.type = RHIBufferType::Upload;
        break;
    case RendererInterface::DEFAULT:
        buffer_desc.type = RHIBufferType::Default;
        break;
    }
    
    switch (desc.usage)
    {
    case RendererInterface::USAGE_VERTEX_BUFFER:
        buffer_desc.usage = RUF_VERTEX_BUFFER; 
        break;
    case RendererInterface::USAGE_INDEX_BUFFER_R32:
    case RendererInterface::USAGE_INDEX_BUFFER_R16:
        buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_INDEX_BUFFER | RUF_TRANSFER_DST);
        break;
    case RendererInterface::USAGE_CBV:
        buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_ALLOW_CBV | RUF_TRANSFER_DST);
        break;
    case RendererInterface::USAGE_UAV:
        buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_TRANSFER_DST);
        break;
    case RendererInterface::USAGE_SRV:
        buffer_desc.usage = static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST);
        break;
    }

    return buffer_desc;
}

bool ResourceManager::InitResourceManager(const RendererInterface::RenderDeviceDesc& desc)
{
    m_device_desc = desc;
    
    m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
    EXIT_WHEN_FALSE(m_factory->InitFactory())  
    
    m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
    EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))
    
    m_command_queue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
    EXIT_WHEN_FALSE(m_command_queue->InitCommandQueue(*m_device))

    const auto& render_window = RendererInterface::InternalResourceHandleTable::Instance().GetRenderWindow(desc.window);
    
    m_swap_chain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
    RHITextureDesc swap_chain_texture_desc("swap_chain_back_buffer",
        render_window.GetWidth(), render_window.GetHeight(),
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_DST),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f}
        });

    RHISwapChainDesc swap_chain_desc;
    swap_chain_desc.hwnd = render_window.GetHWND();
    swap_chain_desc.chain_mode = VSYNC;
    swap_chain_desc.full_screen = false;
    EXIT_WHEN_FALSE(m_swap_chain->InitSwapChain(*m_factory, *m_device, *m_command_queue, swap_chain_texture_desc, swap_chain_desc ))

    m_memory_manager = RHIResourceFactory::CreateRHIResource<IRHIMemoryManager>();
    EXIT_WHEN_FALSE(m_memory_manager->InitMemoryManager(*m_device, *m_factory,
            {
            256,
            64,
            64
            }))
    
    m_command_allocators.resize(desc.back_buffer_count);
    m_command_lists.resize(desc.back_buffer_count);
    
    for (size_t i = 0; i < desc.back_buffer_count; ++i)
    {
        m_command_allocators[i] = RHIResourceFactory::CreateRHIResource<IRHICommandAllocator>();
        m_command_allocators[i]->InitCommandAllocator(*m_device, RHICommandAllocatorType::DIRECT);
        
        m_command_lists[i] = RHIResourceFactory::CreateRHIResource<IRHICommandList>();
        m_command_lists[i]->InitCommandList(*m_device, *m_command_allocators[i]);
    }
    
    m_render_target_manager = RHIResourceFactory::CreateRHIResource<IRHIRenderTargetManager>();
    m_render_target_manager->InitRenderTargetManager(*m_device, 100);

    RHITextureClearValue clear_value
    {
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_memory_manager, *m_swap_chain, clear_value);

    for (unsigned i = 0; i < desc.back_buffer_count; ++i)
    {
        auto depth_texture = m_render_target_manager->CreateRenderTarget(
            *m_device, *m_memory_manager,
            RHITextureDesc::MakeDepthTextureDesc(render_window.GetWidth(), render_window.GetHeight()), RHIDataFormat::D32_FLOAT);
    }
    
    //m_frame_resource_managers.resize(desc.back_buffer_count);

    return true;
}


RendererInterface::BufferHandle ResourceManager::CreateBuffer(const RendererInterface::BufferDesc& desc)
{
    RHIBufferDesc buffer_desc = ConvertToRHIBufferDesc(desc);
    
    std::shared_ptr<IRHIBufferAllocation> buffer_allocation;
    bool allocated = m_memory_manager->AllocateBufferMemory(*m_device, buffer_desc, buffer_allocation);
    GLTF_CHECK(allocated);

    if (desc.data.has_value())
    {
        m_memory_manager->UploadBufferData(*m_device, GetCommandListForRecordPassCommand(), *buffer_allocation, desc.data.value(), 0, desc.size);    
    }
    
    return RendererInterface::InternalResourceHandleTable::Instance().RegisterBuffer(buffer_allocation);
}

RendererInterface::IndexedBufferHandle ResourceManager::CreateIndexedBuffer(const RendererInterface::BufferDesc& desc)
{
    RHIBufferDesc buffer_desc = ConvertToRHIBufferDesc(desc);

    GLTF_CHECK(desc.data.has_value() && (desc.usage == RendererInterface::USAGE_INDEX_BUFFER_R32 || desc.usage == RendererInterface::USAGE_INDEX_BUFFER_R16));
    
    auto index_buffer = std::make_shared<RHIIndexBuffer>();
    IndexBufferData index_buffer_data{};
    index_buffer_data.byte_size = desc.size;
    index_buffer_data.data = std::make_unique<char[]>(index_buffer_data.byte_size);
    index_buffer_data.format = desc.usage == RendererInterface::USAGE_INDEX_BUFFER_R32 ? RHIDataFormat::R32_UINT : RHIDataFormat::R16_UINT;
    index_buffer_data.index_count = index_buffer_data.byte_size / (index_buffer_data.format == RHIDataFormat::R16_UINT ? sizeof(uint16_t) : sizeof(uint32_t));
    
    memcpy(index_buffer_data.data.get(), desc.data.value(),  index_buffer_data.byte_size);
    
    auto index_buffer_view = index_buffer->CreateIndexBufferView(*m_device, *m_memory_manager, GetCommandListForRecordPassCommand(), buffer_desc, index_buffer_data);
    index_buffer->GetBuffer().Transition(GetCommandListForRecordPassCommand(), RHIResourceStateType::STATE_INDEX_BUFFER);
    
    return RendererInterface::InternalResourceHandleTable::Instance().RegisterIndexedBufferAndView(index_buffer_view, index_buffer);
}

RendererInterface::ShaderHandle ResourceManager::CreateShader(const RendererInterface::ShaderDesc& shader_desc)
{
    RHIShaderType shader_type = RHIShaderType::Unknown;
    switch (shader_desc.shader_type) {
    case RendererInterface::VERTEX_SHADER:
        shader_type = RHIShaderType::Vertex;
        break;
    case RendererInterface::FRAGMENT_SHADER:
        shader_type = RHIShaderType::Pixel;
        break;
    case RendererInterface::COMPUTE_SHADER:
        shader_type = RHIShaderType::Compute;
        break;
    case RendererInterface::RAY_TRACING_SHADER:
        shader_type = RHIShaderType::RayTracing;
        break;
    }
    GLTF_CHECK(shader_type != RHIShaderType::Unknown);
    
    std::shared_ptr<IRHIShader> shader = RHIResourceFactory::CreateRHIResource<IRHIShader>();
    if (!shader->InitShader(shader_desc.shader_file_name, shader_type, shader_desc.entry_point) || !shader->CompileShader())
    {
        GLTF_CHECK(false);
    }
    
    RHIUtilInstanceManager::Instance().ProcessShaderMetaData(*shader);
    
    auto shader_handle = RendererInterface::InternalResourceHandleTable::Instance().RegisterShader(shader);
    m_shaders[shader_handle] = shader;
    
    return shader_handle;
}

RendererInterface::RenderTargetHandle ResourceManager::CreateRenderTarget(const RendererInterface::RenderTargetDesc& desc)
{
    RHIDataFormat format = RendererInterfaceRHIConverter::ConvertToRHIFormat(desc.format);
    GLTF_CHECK(format != RHIDataFormat::UNKNOWN);
    bool is_depth_stencil = IsDepthStencilFormat(format);
    
    RHITextureClearValue clear_value{};
    clear_value.clear_format = format;
    if (!is_depth_stencil)
    {
        memcpy(clear_value.clear_color, desc.clear.clear_color, sizeof(desc.clear.clear_color));
    }
    else
    {
        clear_value.clear_depth_stencil.clear_depth = desc.clear.clear_depth_stencil.clear_depth;
        clear_value.clear_depth_stencil.clear_stencil_value = desc.clear.clear_depth_stencil.clear_stencil;
    }

    RHIResourceUsageFlags usage = is_depth_stencil ? RUF_ALLOW_DEPTH_STENCIL : RUF_ALLOW_RENDER_TARGET;
    if (desc.usage & RendererInterface::COPY_SRC)
    {
        usage = RHIResourceUsageFlags(usage | RUF_TRANSFER_SRC);
    }
    if (desc.usage & RendererInterface::COPY_DST)
    {
        usage = RHIResourceUsageFlags(usage | RUF_TRANSFER_DST);
    }
    
    RHITextureDesc tex_desc(desc.name, desc.width, desc.height, format, usage, clear_value);
    auto render_target_descriptor = m_render_target_manager->CreateRenderTarget(*m_device, *m_memory_manager, tex_desc, format);
    auto render_target_handle = RendererInterface::InternalResourceHandleTable::Instance().RegisterRenderTarget(render_target_descriptor);
    m_render_targets[render_target_handle] = render_target_descriptor;
    
    return render_target_handle;
}

unsigned ResourceManager::GetCurrentBackBufferIndex() const
{
    return m_swap_chain->GetCurrentBackBufferIndex();
}

IRHIDevice& ResourceManager::GetDevice()
{
    return *m_device;
}

IRHISwapChain& ResourceManager::GetSwapChain()
{
    return *m_swap_chain;
}

IRHIMemoryManager& ResourceManager::GetMemoryManager()
{
    return *m_memory_manager;
}

IRHICommandList& ResourceManager::GetCommandListForRecordPassCommand(
    RendererInterface::RenderPassHandle render_pass_handle)
{
    auto render_pass = render_pass_handle != NULL_HANDLE ?  RendererInterface::InternalResourceHandleTable::Instance().GetRenderPass(render_pass_handle) : nullptr;
    
    const auto current_frame_index = GetCurrentBackBufferIndex() % m_device_desc.back_buffer_count;
    auto& command_list = *m_command_lists[current_frame_index];
    auto& command_allocator = *m_command_allocators[current_frame_index];

    if (command_list.GetState() == RHICommandListState::Closed)
    {
        const bool reset = RHIUtilInstanceManager::Instance().ResetCommandList(command_list, command_allocator, render_pass ? &render_pass->GetPipelineStateObject() : nullptr);
        GLTF_CHECK(reset);
    }
    else if (render_pass)
    {
        // reset pso
        RHIUtilInstanceManager::Instance().SetPipelineState(command_list, render_pass->GetPipelineStateObject());
    }
    
    return command_list;
}

IRHICommandQueue& ResourceManager::GetCommandQueue()
{
    return *m_command_queue;
}

IRHITextureDescriptorAllocation& ResourceManager::GetCurrentSwapchainRT()
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % m_device_desc.back_buffer_count;
    return *m_swapchain_RTs[current_frame_index];
}

#include "ResourceManager.h"

#include <algorithm>
#include <cmath>

#include "InternalResourceHandleTable.h"
#include "RenderPass.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"
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

    case RendererInterface::RGBA16_FLOAT:
        return RHIDataFormat::R16G16B16A16_FLOAT;

    case RendererInterface::R32_FLOAT:
        return RHIDataFormat::R32_FLOAT;

    case RendererInterface::R16_FLOAT:
        return RHIDataFormat::R16_FLOAT;

    case RendererInterface::R8_UNORM:
        return RHIDataFormat::R8_UNORM;

    case RendererInterface::D32:
        return RHIDataFormat::D32_FLOAT;
    }
    
    GLTF_CHECK(false);
    return RHIDataFormat::UNKNOWN;
}

RHIPipelineType RendererInterfaceRHIConverter::ConvertToRHIPipelineType(RendererInterface::RenderPassType type)
{
    switch (type) {
    case RendererInterface::RenderPassType::GRAPHICS:
        return RHIPipelineType::Graphics;
        
    case RendererInterface::RenderPassType::COMPUTE:
        return RHIPipelineType::Compute;
        
    case RendererInterface::RenderPassType::RAY_TRACING:
        return RHIPipelineType::RayTracing;
        
    }
    GLTF_CHECK(false);
    return RHIPipelineType::Unknown;
}

namespace
{
    RHITextureClearValue BuildRenderTargetClearValue(const RendererInterface::RenderTargetDesc& desc, RHIDataFormat format, bool is_depth_stencil)
    {
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
        return clear_value;
    }

    RHIResourceUsageFlags BuildRenderTargetUsageFlags(const RendererInterface::RenderTargetDesc& desc, bool is_depth_stencil)
    {
        RHIResourceUsageFlags usage = is_depth_stencil ? RUF_ALLOW_DEPTH_STENCIL : RUF_ALLOW_RENDER_TARGET;
        if (desc.usage & RendererInterface::ResourceUsage::COPY_SRC)
        {
            usage = RHIResourceUsageFlags(usage | RUF_TRANSFER_SRC);
        }
        if (desc.usage & RendererInterface::ResourceUsage::COPY_DST)
        {
            usage = RHIResourceUsageFlags(usage | RUF_TRANSFER_DST);
        }
        if (desc.usage & RendererInterface::ResourceUsage::DEPTH_STENCIL)
        {
            usage = RHIResourceUsageFlags(usage | RUF_ALLOW_DEPTH_STENCIL);
        }
        if (desc.usage & RendererInterface::ResourceUsage::SHADER_RESOURCE)
        {
            usage = RHIResourceUsageFlags(usage | RUF_ALLOW_SRV);
        }
        if (desc.usage & RendererInterface::ResourceUsage::UNORDER_ACCESS)
        {
            usage = RHIResourceUsageFlags(usage | RUF_ALLOW_UAV);
        }
        if (desc.enable_mipmaps)
        {
            usage = RHIResourceUsageFlags(usage | RUF_CONTAINS_MIPMAP);
        }
        return usage;
    }

    std::shared_ptr<IRHITextureDescriptorAllocation> CreateRenderTargetAllocation(
        IRHIDevice& device,
        IRHIMemoryManager& memory_manager,
        IRHIRenderTargetManager& render_target_manager,
        const RendererInterface::RenderTargetDesc& desc)
    {
        const RHIDataFormat format = RendererInterfaceRHIConverter::ConvertToRHIFormat(desc.format);
        GLTF_CHECK(format != RHIDataFormat::UNKNOWN);
        const bool is_depth_stencil = IsDepthStencilFormat(format);
        const RHITextureClearValue clear_value = BuildRenderTargetClearValue(desc, format, is_depth_stencil);
        const RHIResourceUsageFlags usage = BuildRenderTargetUsageFlags(desc, is_depth_stencil);
        RHITextureDesc tex_desc(desc.name, desc.width, desc.height, format, usage, clear_value);
        return render_target_manager.CreateRenderTarget(device, memory_manager, tex_desc, format);
    }

    unsigned ComputeWindowRelativeExtent(unsigned window_extent, float scale, unsigned min_extent)
    {
        const float scaled = static_cast<float>(window_extent) * scale;
        const unsigned rounded = static_cast<unsigned>((std::max)(1.0f, std::round(scaled)));
        return (std::max)(min_extent, rounded);
    }
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
            512,
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
    m_last_requested_swapchain_width = render_window.GetWidth();
    m_last_requested_swapchain_height = render_window.GetHeight();

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
    RendererInterface::RenderTargetDesc stored_desc = desc;
    if (stored_desc.size_mode == RendererInterface::RenderTargetSizeMode::FIXED &&
        m_swap_chain &&
        stored_desc.width == m_swap_chain->GetWidth() &&
        stored_desc.height == m_swap_chain->GetHeight())
    {
        // Auto-mark full-resolution RT as window-relative to make runtime resize path robust.
        stored_desc.size_mode = RendererInterface::RenderTargetSizeMode::WINDOW_RELATIVE;
        stored_desc.width_scale = 1.0f;
        stored_desc.height_scale = 1.0f;
    }

    auto render_target_descriptor = CreateRenderTargetAllocation(*m_device, *m_memory_manager, *m_render_target_manager, stored_desc);
    auto render_target_handle = RendererInterface::InternalResourceHandleTable::Instance().RegisterRenderTarget(render_target_descriptor);
    m_render_targets[render_target_handle] = render_target_descriptor;
    m_render_target_descs[render_target_handle] = stored_desc;
    
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

void ResourceManager::WaitFrameRenderFinished()
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % m_device_desc.back_buffer_count;
    auto& command_list = *m_command_lists[current_frame_index];
    auto& command_allocator = *m_command_allocators[current_frame_index];

    if (command_list.GetState() == RHICommandListState::Recording)
    {
        const bool closed = RHIUtilInstanceManager::Instance().CloseCommandList(command_list);
        GLTF_CHECK(closed);

        GLTF_CHECK(RHIUtilInstanceManager::Instance().ExecuteCommandList(command_list, GetCommandQueue(), {}));
        RHIUtilInstanceManager::Instance().WaitCommandListFinish(command_list);

        command_list.SetState(RHICommandListState::Closed);
    }
    
    RHIUtilInstanceManager::Instance().ResetCommandAllocator(command_allocator);
}

void ResourceManager::WaitGPUIdle()
{
    if (m_swap_chain && m_device)
    {
        m_swap_chain->HostWaitPresentFinished(*m_device);
    }
    if (m_command_queue)
    {
        RHIUtilInstanceManager::Instance().WaitCommandQueueIdle(*m_command_queue);
    }
}

void ResourceManager::InvalidateSwapchainResizeRequest()
{
    m_last_requested_swapchain_width = 0;
    m_last_requested_swapchain_height = 0;
}

bool ResourceManager::ResizeSwapchainIfNeeded(unsigned width, unsigned height)
{
    if (!m_swap_chain || !m_factory || !m_device || !m_command_queue || !m_memory_manager || !m_render_target_manager)
    {
        return false;
    }

    if (width == 0 || height == 0)
    {
        return false;
    }

    if (m_last_requested_swapchain_width == width && m_last_requested_swapchain_height == height)
    {
        return false;
    }

    m_last_requested_swapchain_width = width;
    m_last_requested_swapchain_height = height;

    if (m_swap_chain->GetWidth() == width && m_swap_chain->GetHeight() == height)
    {
        return false;
    }

    WaitFrameRenderFinished();
    WaitGPUIdle();

    m_swapchain_RTs.clear();

    const bool released = m_swap_chain->Release(*m_memory_manager);
    GLTF_CHECK(released);

    const auto& render_window = RendererInterface::InternalResourceHandleTable::Instance().GetRenderWindow(m_device_desc.window);
    RHITextureDesc swap_chain_texture_desc("swap_chain_back_buffer",
        width,
        height,
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_DST),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f}
        });

    RHISwapChainDesc swap_chain_desc{};
    swap_chain_desc.hwnd = render_window.GetHWND();
    swap_chain_desc.chain_mode = VSYNC;
    swap_chain_desc.full_screen = false;
    const bool recreated = m_swap_chain->InitSwapChain(*m_factory, *m_device, *m_command_queue, swap_chain_texture_desc, swap_chain_desc);
    GLTF_CHECK(recreated);

    RHITextureClearValue clear_value
    {
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };
    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_memory_manager, *m_swap_chain, clear_value);
    m_last_requested_swapchain_width = width;
    m_last_requested_swapchain_height = height;

    LOG_FORMAT_FLUSH("[ResourceManager] Recreated swapchain (requested=%ux%u, actual=%ux%u)\n",
        width,
        height,
        m_swap_chain->GetWidth(),
        m_swap_chain->GetHeight());
    return true;
}

bool ResourceManager::ResizeWindowDependentRenderTargets(unsigned width, unsigned height)
{
    if (width == 0 || height == 0 || !m_device || !m_memory_manager || !m_render_target_manager)
    {
        return false;
    }

    std::vector<std::pair<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetDesc>> resize_targets;
    resize_targets.reserve(m_render_target_descs.size());
    for (const auto& render_target_desc_pair : m_render_target_descs)
    {
        const auto handle = render_target_desc_pair.first;
        const auto& desc = render_target_desc_pair.second;
        if (desc.size_mode != RendererInterface::RenderTargetSizeMode::WINDOW_RELATIVE)
        {
            continue;
        }

        const unsigned target_width = ComputeWindowRelativeExtent(width, desc.width_scale, desc.min_width);
        const unsigned target_height = ComputeWindowRelativeExtent(height, desc.height_scale, desc.min_height);
        if (desc.width == target_width && desc.height == target_height)
        {
            continue;
        }

        RendererInterface::RenderTargetDesc resized_desc = desc;
        resized_desc.width = target_width;
        resized_desc.height = target_height;
        resize_targets.emplace_back(handle, resized_desc);
    }

    if (resize_targets.empty())
    {
        return false;
    }

    WaitFrameRenderFinished();
    WaitGPUIdle();

    for (const auto& resize_target : resize_targets)
    {
        const auto handle = resize_target.first;
        const auto& resized_desc = resize_target.second;
        auto resized_allocation = CreateRenderTargetAllocation(*m_device, *m_memory_manager, *m_render_target_manager, resized_desc);
        m_render_targets[handle] = resized_allocation;
        m_render_target_descs[handle] = resized_desc;
        const bool updated = RendererInterface::InternalResourceHandleTable::Instance().UpdateRenderTarget(handle, resized_allocation);
        GLTF_CHECK(updated);

        LOG_FORMAT_FLUSH("[ResourceManager] Resized RT '%s' to %ux%u\n",
            resized_desc.name.c_str(),
            resized_desc.width,
            resized_desc.height);
    }

    return true;
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
        const bool reset_command_list = RHIUtilInstanceManager::Instance().ResetCommandList(command_list, command_allocator, render_pass ? &render_pass->GetPipelineStateObject() : nullptr);
        GLTF_CHECK(reset_command_list);
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
    GLTF_CHECK(!m_swapchain_RTs.empty());
    const auto swapchain_image_index = m_swap_chain->GetCurrentSwapchainImageIndex() % static_cast<unsigned>(m_swapchain_RTs.size());
    return *m_swapchain_RTs[swapchain_image_index];
}

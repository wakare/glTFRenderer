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
    const char* ToString(RendererInterface::SwapchainLifecycleState state)
    {
        switch (state)
        {
        case RendererInterface::SwapchainLifecycleState::UNINITIALIZED:
            return "UNINITIALIZED";
        case RendererInterface::SwapchainLifecycleState::READY:
            return "READY";
        case RendererInterface::SwapchainLifecycleState::RESIZE_PENDING:
            return "RESIZE_PENDING";
        case RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED:
            return "RESIZE_DEFERRED";
        case RendererInterface::SwapchainLifecycleState::RECOVERING:
            return "RECOVERING";
        case RendererInterface::SwapchainLifecycleState::MINIMIZED:
            return "MINIMIZED";
        case RendererInterface::SwapchainLifecycleState::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

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
    SetSwapchainResizePolicy(desc.swapchain_resize_policy, false);
    
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
    m_last_observed_window_width = render_window.GetWidth();
    m_last_observed_window_height = render_window.GetHeight();
    m_resize_request_stable_frame_count = 1;
    m_deferred_release_frame_index = 0;
    SetSwapchainLifecycleState(
        m_swapchain_RTs.empty()
            ? RendererInterface::SwapchainLifecycleState::INVALID
            : RendererInterface::SwapchainLifecycleState::READY,
        "resource manager initialized");

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
    for (const auto& command_list : m_command_lists)
    {
        if (command_list)
        {
            if (command_list->GetState() == RHICommandListState::Recording)
            {
                RHIUtilInstanceManager::Instance().CloseCommandList(*command_list);
            }
            RHIUtilInstanceManager::Instance().WaitCommandListFinish(*command_list);
        }
    }
    if (m_swap_chain && m_device)
    {
        m_swap_chain->HostWaitPresentFinished(*m_device);
    }
    if (m_command_queue)
    {
        RHIUtilInstanceManager::Instance().WaitCommandQueueIdle(*m_command_queue);
    }

    const auto frame_slot_count = (std::min)(m_command_lists.size(), m_command_allocators.size());
    for (size_t frame_slot = 0; frame_slot < frame_slot_count; ++frame_slot)
    {
        auto& command_list = m_command_lists[frame_slot];
        auto& command_allocator = m_command_allocators[frame_slot];
        if (!command_list || !command_allocator)
        {
            continue;
        }

        RHIUtilInstanceManager::Instance().ResetCommandAllocator(*command_allocator);
        const bool reset = RHIUtilInstanceManager::Instance().ResetCommandList(*command_list, *command_allocator, nullptr);
        GLTF_CHECK(reset);
        const bool closed = RHIUtilInstanceManager::Instance().CloseCommandList(*command_list);
        GLTF_CHECK(closed);
    }
}

void ResourceManager::SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState state, const char* reason)
{
    if (m_swapchain_lifecycle_state == state)
    {
        return;
    }

    const auto previous_state = m_swapchain_lifecycle_state;
    m_swapchain_lifecycle_state = state;
    if (reason && reason[0] != '\0')
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain state: %s -> %s (%s)\n",
            ToString(previous_state),
            ToString(m_swapchain_lifecycle_state),
            reason);
    }
    else
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain state: %s -> %s\n",
            ToString(previous_state),
            ToString(m_swapchain_lifecycle_state));
    }
}

unsigned ResourceManager::GetDeferredReleaseLatencyFrames() const
{
    const unsigned back_buffer_count = m_swap_chain ? m_swap_chain->GetBackBufferCount() : 0;
    return (std::max)(2u, back_buffer_count + 1u);
}

unsigned ResourceManager::ComputeRetryCooldownFrames(unsigned failure_count) const
{
    const auto& policy = m_device_desc.swapchain_resize_policy;
    const unsigned base_frames = (std::max)(1u, policy.retry_cooldown_base_frames);
    const unsigned max_frames = (std::max)(base_frames, policy.retry_cooldown_max_frames);
    if (failure_count <= 1 || !policy.use_exponential_retry_backoff)
    {
        return base_frames;
    }

    unsigned cooldown = base_frames;
    const unsigned step_count = (std::min)(failure_count - 1u, 10u);
    for (unsigned step = 0; step < step_count; ++step)
    {
        if (cooldown >= max_frames)
        {
            return max_frames;
        }
        cooldown = (std::min)(max_frames, cooldown * 2u);
    }
    return cooldown;
}

unsigned ResourceManager::GetRetryLogPeriod() const
{
    return (std::max)(1u, m_device_desc.swapchain_resize_policy.retry_log_period);
}

void ResourceManager::UpdateResizeRequestStability(unsigned width, unsigned height)
{
    if (m_last_observed_window_width != width || m_last_observed_window_height != height)
    {
        m_last_observed_window_width = width;
        m_last_observed_window_height = height;
        m_resize_request_stable_frame_count = 1;
        return;
    }
    ++m_resize_request_stable_frame_count;
}

bool ResourceManager::IsResizeRequestStableEnough(unsigned width, unsigned height, bool pending_retry_for_same_size) const
{
    if (m_swapchain_acquire_failure_count > 0 || m_swapchain_present_failure_count > 0)
    {
        return true;
    }

    if (pending_retry_for_same_size)
    {
        return true;
    }

    const unsigned required_stable_frames = (std::max)(1u, m_device_desc.swapchain_resize_policy.min_stable_frames_before_resize);
    if (required_stable_frames <= 1)
    {
        return true;
    }

    if (m_last_observed_window_width != width || m_last_observed_window_height != height)
    {
        return false;
    }
    return m_resize_request_stable_frame_count >= required_stable_frames;
}

void ResourceManager::EnqueueResourceForDeferredRelease(const std::shared_ptr<IRHIResource>& resource)
{
    if (!resource)
    {
        return;
    }

    const unsigned delay_frames = GetDeferredReleaseLatencyFrames();
    const unsigned long long retire_frame = m_deferred_release_frame_index + delay_frames;
    if (!m_deferred_release_entries.empty() && m_deferred_release_entries.back().retire_frame == retire_frame)
    {
        m_deferred_release_entries.back().resources.push_back(resource);
        return;
    }

    DeferredReleaseEntry entry{};
    entry.retire_frame = retire_frame;
    entry.resources.push_back(resource);
    m_deferred_release_entries.push_back(std::move(entry));
}

void ResourceManager::FlushDeferredResourceReleases(bool force_release_all)
{
    if (!m_memory_manager)
    {
        m_deferred_release_entries.clear();
        return;
    }

    while (!m_deferred_release_entries.empty())
    {
        if (!force_release_all && m_deferred_release_entries.front().retire_frame > m_deferred_release_frame_index)
        {
            break;
        }

        auto entry = std::move(m_deferred_release_entries.front());
        m_deferred_release_entries.pop_front();
        for (const auto& resource : entry.resources)
        {
            const bool released = RHIResourceFactory::ReleaseResource(*m_memory_manager, resource);
            GLTF_CHECK(released);
        }
    }
}

void ResourceManager::AdvanceDeferredReleaseFrame()
{
    ++m_deferred_release_frame_index;
    FlushDeferredResourceReleases(false);
}

RendererInterface::WindowSurfaceSyncResult ResourceManager::SyncWindowSurface(unsigned window_width, unsigned window_height)
{
    AdvanceDeferredReleaseFrame();

    RendererInterface::WindowSurfaceSyncResult result{};
    result.window_width = window_width;
    result.window_height = window_height;
    result.lifecycle_state = m_swapchain_lifecycle_state;

    if (!m_swap_chain || !m_factory || !m_device || !m_command_queue || !m_memory_manager || !m_render_target_manager)
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "sync failed: missing runtime objects");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = m_swapchain_lifecycle_state;
        return result;
    }

    if (window_width == 0 || window_height == 0)
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::MINIMIZED, "window extent is zero");
        result.status = RendererInterface::WindowSurfaceSyncStatus::MINIMIZED;
        result.lifecycle_state = m_swapchain_lifecycle_state;
        return result;
    }

    result.swapchain_resized = ResizeSwapchainIfNeeded(window_width, window_height);
    result.render_width = m_swap_chain->GetWidth();
    result.render_height = m_swap_chain->GetHeight();

    if (!HasCurrentSwapchainRT())
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "no current swapchain render target");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = m_swapchain_lifecycle_state;
        return result;
    }

    result.window_render_targets_resized =
        ResizeWindowDependentRenderTargetsImpl(result.render_width, result.render_height, result.swapchain_resized);

    if (!HasCurrentSwapchainRT())
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "window RT resize invalidated swapchain RT");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = m_swapchain_lifecycle_state;
        return result;
    }

    if (m_swapchain_lifecycle_state == RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED)
    {
        result.status = RendererInterface::WindowSurfaceSyncStatus::DEFERRED_RETRY;
        result.lifecycle_state = m_swapchain_lifecycle_state;
        return result;
    }

    if (m_swapchain_acquire_failure_count > 0 || m_swapchain_present_failure_count > 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain recovered (acquire_failures=%u, present_failures=%u).\n",
            m_swapchain_acquire_failure_count,
            m_swapchain_present_failure_count);
        m_swapchain_acquire_failure_count = 0;
        m_swapchain_present_failure_count = 0;
    }

    result.status =
        (result.swapchain_resized || result.window_render_targets_resized)
            ? RendererInterface::WindowSurfaceSyncStatus::RESIZED
            : RendererInterface::WindowSurfaceSyncStatus::READY;
    if (m_swapchain_lifecycle_state != RendererInterface::SwapchainLifecycleState::READY)
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "window surface synchronized");
    }
    result.lifecycle_state = m_swapchain_lifecycle_state;
    return result;
}

void ResourceManager::NotifySwapchainAcquireFailure()
{
    ++m_swapchain_acquire_failure_count;
    const unsigned log_period = GetRetryLogPeriod();
    if (m_swapchain_acquire_failure_count == 1 || (m_swapchain_acquire_failure_count % log_period) == 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain acquire failed (count=%u), schedule resize sync.\n",
            m_swapchain_acquire_failure_count);
    }
    m_last_requested_swapchain_width = 0;
    m_last_requested_swapchain_height = 0;
    m_swapchain_resize_retry_countdown_frames = 0;
    SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "acquire failed");
}

void ResourceManager::NotifySwapchainPresentFailure()
{
    ++m_swapchain_present_failure_count;
    const unsigned log_period = GetRetryLogPeriod();
    if (m_swapchain_present_failure_count == 1 || (m_swapchain_present_failure_count % log_period) == 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain present failed (count=%u), schedule resize sync.\n",
            m_swapchain_present_failure_count);
    }
    m_last_requested_swapchain_width = 0;
    m_last_requested_swapchain_height = 0;
    m_swapchain_resize_retry_countdown_frames = 0;
    SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "present failed");
}

void ResourceManager::InvalidateSwapchainResizeRequest()
{
    FlushDeferredResourceReleases(true);
    m_last_requested_swapchain_width = 0;
    m_last_requested_swapchain_height = 0;
    m_last_observed_window_width = 0;
    m_last_observed_window_height = 0;
    m_resize_request_stable_frame_count = 0;
    m_swapchain_resize_retry_countdown_frames = 0;
    m_swapchain_resize_failure_count = 0;
    m_swapchain_resize_last_failed_width = 0;
    m_swapchain_resize_last_failed_height = 0;
    m_swapchain_acquire_failure_count = 0;
    m_swapchain_present_failure_count = 0;
    SetSwapchainLifecycleState(
        HasCurrentSwapchainRT() ? RendererInterface::SwapchainLifecycleState::RESIZE_PENDING
                                : RendererInterface::SwapchainLifecycleState::INVALID,
        "invalidate resize request");
}

bool ResourceManager::ResizeSwapchainIfNeeded(unsigned width, unsigned height)
{
    if (!m_swap_chain || !m_factory || !m_device || !m_command_queue || !m_memory_manager || !m_render_target_manager)
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "resize failed: missing runtime objects");
        return false;
    }

    if (width == 0 || height == 0)
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::MINIMIZED, "resize skipped: zero window extent");
        return false;
    }

    UpdateResizeRequestStability(width, height);

    if (m_swap_chain->GetWidth() == width && m_swap_chain->GetHeight() == height)
    {
        m_swapchain_resize_retry_countdown_frames = 0;
        m_swapchain_resize_failure_count = 0;
        m_swapchain_resize_last_failed_width = 0;
        m_swapchain_resize_last_failed_height = 0;
        m_last_requested_swapchain_width = width;
        m_last_requested_swapchain_height = height;
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "swapchain extent already up to date");
        return false;
    }

    if (m_swapchain_resize_failure_count > 0 &&
        (m_swapchain_resize_last_failed_width != width || m_swapchain_resize_last_failed_height != height))
    {
        m_swapchain_resize_failure_count = 0;
        m_swapchain_resize_retry_countdown_frames = 0;
        m_swapchain_resize_last_failed_width = 0;
        m_swapchain_resize_last_failed_height = 0;
    }

    const bool pending_retry_for_same_size =
        m_swapchain_resize_failure_count > 0 &&
        m_swapchain_resize_last_failed_width == width &&
        m_swapchain_resize_last_failed_height == height;
    if (pending_retry_for_same_size && m_swapchain_resize_retry_countdown_frames > 0)
    {
        --m_swapchain_resize_retry_countdown_frames;
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize deferred by retry cooldown");
        return false;
    }

    if (!IsResizeRequestStableEnough(width, height, pending_retry_for_same_size))
    {
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize deferred until window extent becomes stable");
        return false;
    }

    m_last_requested_swapchain_width = width;
    m_last_requested_swapchain_height = height;

    SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "start swapchain resize");
    WaitFrameRenderFinished();
    WaitGPUIdle();
    FlushDeferredResourceReleases(true);

    m_swapchain_RTs.clear();
    const bool released_swapchain_rts = m_render_target_manager->ReleaseSwapchainRenderTargets(*m_memory_manager);
    if (!released_swapchain_rts)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Failed to release old swapchain render targets for %ux%u.\n", width, height);
        InvalidateSwapchainResizeRequest();
        return false;
    }

    // DX12 flip-model swapchain resize requires all queued presents and backbuffer refs retired.
    // Wait one more time after releasing CPU-side swapchain RT wrappers to reduce false resize failures.
    if (m_device_desc.type == RendererInterface::DX12)
    {
        m_swap_chain->HostWaitPresentFinished(*m_device);
    }
    RHITextureClearValue clear_value
    {
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    bool swapchain_resized_in_place = m_swap_chain->ResizeSwapChain(width, height);

    if (!swapchain_resized_in_place)
    {
        if (m_device_desc.type == RendererInterface::DX12)
        {
            // Keep the existing swapchain alive and restore descriptors, then retry resize next frame.
            ++m_swapchain_resize_failure_count;
            m_swapchain_resize_last_failed_width = width;
            m_swapchain_resize_last_failed_height = height;
            m_swapchain_resize_retry_countdown_frames = ComputeRetryCooldownFrames(m_swapchain_resize_failure_count);
            const unsigned log_period = GetRetryLogPeriod();
            if (m_swapchain_resize_failure_count == 1 || (m_swapchain_resize_failure_count % log_period) == 0)
            {
                LOG_FORMAT_FLUSH("[ResourceManager] ResizeSwapChain in-place failed for %ux%u (retry_count=%u, cooldown=%u), restore current swapchain RTs and retry later.\n",
                    width,
                    height,
                    m_swapchain_resize_failure_count,
                    m_swapchain_resize_retry_countdown_frames);
            }
            m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_memory_manager, *m_swap_chain, clear_value);
            if (m_swapchain_RTs.empty())
            {
                LOG_FORMAT_FLUSH("[ResourceManager] Failed to restore current swapchain render targets after resize failure.\n");
                InvalidateSwapchainResizeRequest();
                return false;
            }
            SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize in-place failed, waiting for retry");
            return false;
        }

        LOG_FORMAT_FLUSH("[ResourceManager] ResizeSwapChain in-place failed, fallback to recreate for %ux%u.\n", width, height);
        SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RECOVERING, "fallback to recreate swapchain");
        const bool released = m_swap_chain->Release(*m_memory_manager);
        if (!released)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Failed to release swapchain during resize to %ux%u.\n", width, height);
            InvalidateSwapchainResizeRequest();
            return false;
        }

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
        if (!recreated)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Failed to recreate swapchain for %ux%u.\n", width, height);
            InvalidateSwapchainResizeRequest();
            return false;
        }
    }
    else
    {
        if (m_swapchain_resize_failure_count > 0)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Swapchain resize recovered after %u retries.\n", m_swapchain_resize_failure_count);
        }
        m_swapchain_resize_retry_countdown_frames = 0;
        m_swapchain_resize_failure_count = 0;
        m_swapchain_resize_last_failed_width = 0;
        m_swapchain_resize_last_failed_height = 0;
        LOG_FORMAT_FLUSH("[ResourceManager] Resized swapchain in-place to %ux%u.\n", width, height);
    }

    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_memory_manager, *m_swap_chain, clear_value);
    if (m_swapchain_RTs.empty())
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Failed to rebuild swapchain render targets for %ux%u.\n", width, height);
        InvalidateSwapchainResizeRequest();
        return false;
    }
    m_swapchain_resize_retry_countdown_frames = 0;
    m_swapchain_resize_failure_count = 0;
    m_swapchain_resize_last_failed_width = 0;
    m_swapchain_resize_last_failed_height = 0;
    m_last_requested_swapchain_width = width;
    m_last_requested_swapchain_height = height;
    SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "swapchain resize committed");

    LOG_FORMAT_FLUSH("[ResourceManager] Updated swapchain resources (requested=%ux%u, actual=%ux%u)\n",
        width,
        height,
        m_swap_chain->GetWidth(),
        m_swap_chain->GetHeight());
    return true;
}

bool ResourceManager::ResizeWindowDependentRenderTargets(unsigned width, unsigned height)
{
    return ResizeWindowDependentRenderTargetsImpl(width, height, false);
}

bool ResourceManager::ResizeWindowDependentRenderTargetsImpl(unsigned width, unsigned height, bool assume_gpu_idle)
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

    if (!assume_gpu_idle)
    {
        WaitFrameRenderFinished();
    }

    for (const auto& resize_target : resize_targets)
    {
        const auto handle = resize_target.first;
        const auto& resized_desc = resize_target.second;
        std::shared_ptr<IRHITextureDescriptorAllocation> old_allocation = nullptr;
        const auto old_rt_it = m_render_targets.find(handle);
        if (old_rt_it != m_render_targets.end())
        {
            old_allocation = old_rt_it->second;
        }
        auto resized_allocation = CreateRenderTargetAllocation(*m_device, *m_memory_manager, *m_render_target_manager, resized_desc);
        m_render_targets[handle] = resized_allocation;
        m_render_target_descs[handle] = resized_desc;
        const bool updated = RendererInterface::InternalResourceHandleTable::Instance().UpdateRenderTarget(handle, resized_allocation);
        GLTF_CHECK(updated);

        if (old_allocation && old_allocation != resized_allocation)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(old_allocation));
        }

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

bool ResourceManager::HasCurrentSwapchainRT() const
{
    return m_swap_chain && !m_swapchain_RTs.empty();
}

RendererInterface::SwapchainLifecycleState ResourceManager::GetSwapchainLifecycleState() const
{
    return m_swapchain_lifecycle_state;
}

RendererInterface::SwapchainResizePolicy ResourceManager::GetSwapchainResizePolicy() const
{
    return m_device_desc.swapchain_resize_policy;
}

void ResourceManager::SetSwapchainResizePolicy(const RendererInterface::SwapchainResizePolicy& policy, bool reset_retry_state)
{
    auto normalized_policy = policy;
    normalized_policy.min_stable_frames_before_resize = (std::max)(1u, normalized_policy.min_stable_frames_before_resize);
    normalized_policy.retry_cooldown_base_frames = (std::max)(1u, normalized_policy.retry_cooldown_base_frames);
    normalized_policy.retry_cooldown_max_frames = (std::max)(normalized_policy.retry_cooldown_base_frames, normalized_policy.retry_cooldown_max_frames);
    normalized_policy.retry_log_period = (std::max)(1u, normalized_policy.retry_log_period);
    m_device_desc.swapchain_resize_policy = normalized_policy;

    if (reset_retry_state)
    {
        m_swapchain_resize_retry_countdown_frames = 0;
        m_swapchain_resize_failure_count = 0;
        m_swapchain_resize_last_failed_width = 0;
        m_swapchain_resize_last_failed_height = 0;
    }
}

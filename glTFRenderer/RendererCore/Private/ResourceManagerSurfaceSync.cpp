#include "ResourceManagerSurfaceSync.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "InternalResourceHandleTable.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIRenderTargetManager.h"
#include "RHIInterface/IRHISwapChain.h"

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

bool ResourceManagerSurfaceResourceRebuilder::ResizeSwapchainIfNeeded(ResourceManager& manager, unsigned width, unsigned height)
{
    auto reset_swapchain_resize_retry_state = [&manager]()
    {
        manager.m_swapchain_resize_retry_countdown_frames = 0;
        manager.m_swapchain_resize_failure_count = 0;
        manager.m_swapchain_resize_last_failed_width = 0;
        manager.m_swapchain_resize_last_failed_height = 0;
    };

    if (!manager.m_swap_chain || !manager.m_factory || !manager.m_device || !manager.m_command_queue || !manager.m_memory_manager || !manager.m_render_target_manager)
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "resize failed: missing runtime objects");
        return false;
    }

    if (width == 0 || height == 0)
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::MINIMIZED, "resize skipped: zero window extent");
        return false;
    }

    manager.UpdateResizeRequestStability(width, height);

    if (manager.m_swap_chain->GetWidth() == width && manager.m_swap_chain->GetHeight() == height)
    {
        reset_swapchain_resize_retry_state();
        manager.m_last_requested_swapchain_width = width;
        manager.m_last_requested_swapchain_height = height;
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "swapchain extent already up to date");
        return false;
    }

    if (manager.m_swapchain_resize_failure_count > 0 &&
        (manager.m_swapchain_resize_last_failed_width != width || manager.m_swapchain_resize_last_failed_height != height))
    {
        reset_swapchain_resize_retry_state();
    }

    const bool pending_retry_for_same_size =
        manager.m_swapchain_resize_failure_count > 0 &&
        manager.m_swapchain_resize_last_failed_width == width &&
        manager.m_swapchain_resize_last_failed_height == height;
    if (pending_retry_for_same_size && manager.m_swapchain_resize_retry_countdown_frames > 0)
    {
        --manager.m_swapchain_resize_retry_countdown_frames;
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize deferred by retry cooldown");
        return false;
    }

    if (!manager.IsResizeRequestStableEnough(width, height, pending_retry_for_same_size))
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize deferred until window extent becomes stable");
        return false;
    }

    manager.m_last_requested_swapchain_width = width;
    manager.m_last_requested_swapchain_height = height;

    manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "start swapchain resize");
    manager.WaitFrameRenderFinished();
    manager.WaitGPUIdle();
    manager.FlushDeferredResourceReleases(true);

    manager.m_swapchain_RTs.clear();
    const bool released_swapchain_rts = manager.m_render_target_manager->ReleaseSwapchainRenderTargets(*manager.m_memory_manager);
    if (!released_swapchain_rts)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Failed to release old swapchain render targets for %ux%u.\n", width, height);
        manager.InvalidateSwapchainResizeRequest();
        return false;
    }

    // DX12 flip-model swapchain resize requires all queued presents and backbuffer refs retired.
    // Wait one more time after releasing CPU-side swapchain RT wrappers to reduce false resize failures.
    if (manager.m_device_desc.type == RendererInterface::DX12)
    {
        manager.m_swap_chain->HostWaitPresentFinished(*manager.m_device);
    }
    RHITextureClearValue clear_value
    {
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    const bool swapchain_resized_in_place = manager.m_swap_chain->ResizeSwapChain(width, height);

    if (!swapchain_resized_in_place)
    {
        if (manager.m_device_desc.type == RendererInterface::DX12)
        {
            // Keep the existing swapchain alive and restore descriptors, then retry resize next frame.
            ++manager.m_swapchain_resize_failure_count;
            manager.m_swapchain_resize_last_failed_width = width;
            manager.m_swapchain_resize_last_failed_height = height;
            manager.m_swapchain_resize_retry_countdown_frames = manager.ComputeRetryCooldownFrames(manager.m_swapchain_resize_failure_count);
            const unsigned log_period = manager.GetRetryLogPeriod();
            if (manager.m_swapchain_resize_failure_count == 1 || (manager.m_swapchain_resize_failure_count % log_period) == 0)
            {
                LOG_FORMAT_FLUSH("[ResourceManager] ResizeSwapChain in-place failed for %ux%u (retry_count=%u, cooldown=%u), restore current swapchain RTs and retry later.\n",
                    width,
                    height,
                    manager.m_swapchain_resize_failure_count,
                    manager.m_swapchain_resize_retry_countdown_frames);
            }
            manager.m_swapchain_RTs = manager.m_render_target_manager->CreateRenderTargetFromSwapChain(*manager.m_device, *manager.m_memory_manager, *manager.m_swap_chain, clear_value);
            if (manager.m_swapchain_RTs.empty())
            {
                LOG_FORMAT_FLUSH("[ResourceManager] Failed to restore current swapchain render targets after resize failure.\n");
                manager.InvalidateSwapchainResizeRequest();
                return false;
            }
            manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED, "resize in-place failed, waiting for retry");
            return false;
        }

        LOG_FORMAT_FLUSH("[ResourceManager] ResizeSwapChain in-place failed, fallback to recreate for %ux%u.\n", width, height);
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RECOVERING, "fallback to recreate swapchain");
        const bool released = manager.m_swap_chain->Release(*manager.m_memory_manager);
        if (!released)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Failed to release swapchain during resize to %ux%u.\n", width, height);
            manager.InvalidateSwapchainResizeRequest();
            return false;
        }

        const auto& render_window = RendererInterface::InternalResourceHandleTable::Instance().GetRenderWindow(manager.m_device_desc.window);
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
        const bool recreated = manager.m_swap_chain->InitSwapChain(*manager.m_factory, *manager.m_device, *manager.m_command_queue, swap_chain_texture_desc, swap_chain_desc);
        if (!recreated)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Failed to recreate swapchain for %ux%u.\n", width, height);
            manager.InvalidateSwapchainResizeRequest();
            return false;
        }
    }
    else
    {
        if (manager.m_swapchain_resize_failure_count > 0)
        {
            LOG_FORMAT_FLUSH("[ResourceManager] Swapchain resize recovered after %u retries.\n", manager.m_swapchain_resize_failure_count);
        }
        reset_swapchain_resize_retry_state();
        LOG_FORMAT_FLUSH("[ResourceManager] Resized swapchain in-place to %ux%u.\n", width, height);
    }

    manager.m_swapchain_RTs = manager.m_render_target_manager->CreateRenderTargetFromSwapChain(*manager.m_device, *manager.m_memory_manager, *manager.m_swap_chain, clear_value);
    if (manager.m_swapchain_RTs.empty())
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Failed to rebuild swapchain render targets for %ux%u.\n", width, height);
        manager.InvalidateSwapchainResizeRequest();
        return false;
    }
    reset_swapchain_resize_retry_state();
    manager.m_last_requested_swapchain_width = width;
    manager.m_last_requested_swapchain_height = height;
    manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "swapchain resize committed");

    LOG_FORMAT_FLUSH("[ResourceManager] Updated swapchain resources (requested=%ux%u, actual=%ux%u)\n",
        width,
        height,
        manager.m_swap_chain->GetWidth(),
        manager.m_swap_chain->GetHeight());
    return true;
}

bool ResourceManagerSurfaceResourceRebuilder::ResizeWindowDependentRenderTargets(ResourceManager& manager, unsigned width, unsigned height, bool assume_gpu_idle)
{
    if (width == 0 || height == 0 || !manager.m_device || !manager.m_memory_manager || !manager.m_render_target_manager)
    {
        return false;
    }

    std::vector<std::pair<RendererInterface::RenderTargetHandle, RendererInterface::RenderTargetDesc>> resize_targets;
    resize_targets.reserve(manager.m_render_target_descs.size());
    for (const auto& render_target_desc_pair : manager.m_render_target_descs)
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
        manager.WaitFrameRenderFinished();
    }

    for (const auto& resize_target : resize_targets)
    {
        const auto handle = resize_target.first;
        const auto& resized_desc = resize_target.second;
        std::shared_ptr<IRHITextureDescriptorAllocation> old_allocation = nullptr;
        const auto old_rt_it = manager.m_render_targets.find(handle);
        if (old_rt_it != manager.m_render_targets.end())
        {
            old_allocation = old_rt_it->second;
        }
        auto resized_allocation = CreateRenderTargetAllocation(*manager.m_device, *manager.m_memory_manager, *manager.m_render_target_manager, resized_desc);
        manager.m_render_targets[handle] = resized_allocation;
        manager.m_render_target_descs[handle] = resized_desc;
        const bool updated = RendererInterface::InternalResourceHandleTable::Instance().UpdateRenderTarget(handle, resized_allocation);
        GLTF_CHECK(updated);

        if (old_allocation && old_allocation != resized_allocation)
        {
            manager.EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(old_allocation));
        }

        LOG_FORMAT_FLUSH("[ResourceManager] Resized RT '%s' to %ux%u\n",
            resized_desc.name.c_str(),
            resized_desc.width,
            resized_desc.height);
    }

    return true;
}

RendererInterface::WindowSurfaceSyncResult ResourceManagerSurfaceResizeCoordinator::Sync(ResourceManager& manager, unsigned window_width, unsigned window_height)
{
    manager.AdvanceDeferredReleaseFrame();

    RendererInterface::WindowSurfaceSyncResult result{};
    result.window_width = window_width;
    result.window_height = window_height;
    result.lifecycle_state = manager.m_swapchain_lifecycle_state;

    if (!manager.m_swap_chain || !manager.m_factory || !manager.m_device || !manager.m_command_queue || !manager.m_memory_manager || !manager.m_render_target_manager)
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "sync failed: missing runtime objects");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = manager.m_swapchain_lifecycle_state;
        return result;
    }

    if (window_width == 0 || window_height == 0)
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::MINIMIZED, "window extent is zero");
        result.status = RendererInterface::WindowSurfaceSyncStatus::MINIMIZED;
        result.lifecycle_state = manager.m_swapchain_lifecycle_state;
        return result;
    }

    result.swapchain_resized = ResourceManagerSurfaceResourceRebuilder::ResizeSwapchainIfNeeded(manager, window_width, window_height);
    result.render_width = manager.m_swap_chain->GetWidth();
    result.render_height = manager.m_swap_chain->GetHeight();

    if (!manager.HasCurrentSwapchainRT())
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "no current swapchain render target");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = manager.m_swapchain_lifecycle_state;
        return result;
    }

    result.window_render_targets_resized =
        ResourceManagerSurfaceResourceRebuilder::ResizeWindowDependentRenderTargets(manager, result.render_width, result.render_height, result.swapchain_resized);

    if (!manager.HasCurrentSwapchainRT())
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::INVALID, "window RT resize invalidated swapchain RT");
        result.status = RendererInterface::WindowSurfaceSyncStatus::INVALID;
        result.lifecycle_state = manager.m_swapchain_lifecycle_state;
        return result;
    }

    if (manager.m_swapchain_lifecycle_state == RendererInterface::SwapchainLifecycleState::RESIZE_DEFERRED)
    {
        result.status = RendererInterface::WindowSurfaceSyncStatus::DEFERRED_RETRY;
        result.lifecycle_state = manager.m_swapchain_lifecycle_state;
        return result;
    }

    if (manager.m_swapchain_acquire_failure_count > 0 || manager.m_swapchain_present_failure_count > 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain recovered (acquire_failures=%u, present_failures=%u).\n",
            manager.m_swapchain_acquire_failure_count,
            manager.m_swapchain_present_failure_count);
        manager.m_swapchain_acquire_failure_count = 0;
        manager.m_swapchain_present_failure_count = 0;
    }

    result.status =
        (result.swapchain_resized || result.window_render_targets_resized)
            ? RendererInterface::WindowSurfaceSyncStatus::RESIZED
            : RendererInterface::WindowSurfaceSyncStatus::READY;
    if (manager.m_swapchain_lifecycle_state != RendererInterface::SwapchainLifecycleState::READY)
    {
        manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::READY, "window surface synchronized");
    }
    result.lifecycle_state = manager.m_swapchain_lifecycle_state;
    return result;
}

void ResourceManagerSurfaceResizeCoordinator::NotifySwapchainAcquireFailure(ResourceManager& manager)
{
    ++manager.m_swapchain_acquire_failure_count;
    const unsigned log_period = manager.GetRetryLogPeriod();
    if (manager.m_swapchain_acquire_failure_count == 1 || (manager.m_swapchain_acquire_failure_count % log_period) == 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain acquire failed (count=%u), schedule resize sync.\n",
            manager.m_swapchain_acquire_failure_count);
    }
    manager.m_last_requested_swapchain_width = 0;
    manager.m_last_requested_swapchain_height = 0;
    manager.m_swapchain_resize_retry_countdown_frames = 0;
    manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "acquire failed");
}

void ResourceManagerSurfaceResizeCoordinator::NotifySwapchainPresentFailure(ResourceManager& manager)
{
    ++manager.m_swapchain_present_failure_count;
    const unsigned log_period = manager.GetRetryLogPeriod();
    if (manager.m_swapchain_present_failure_count == 1 || (manager.m_swapchain_present_failure_count % log_period) == 0)
    {
        LOG_FORMAT_FLUSH("[ResourceManager] Swapchain present failed (count=%u), schedule resize sync.\n",
            manager.m_swapchain_present_failure_count);
    }
    manager.m_last_requested_swapchain_width = 0;
    manager.m_last_requested_swapchain_height = 0;
    manager.m_swapchain_resize_retry_countdown_frames = 0;
    manager.SetSwapchainLifecycleState(RendererInterface::SwapchainLifecycleState::RESIZE_PENDING, "present failed");
}

void ResourceManagerSurfaceResizeCoordinator::InvalidateSwapchainResizeRequest(ResourceManager& manager)
{
    manager.FlushDeferredResourceReleases(true);
    manager.m_last_requested_swapchain_width = 0;
    manager.m_last_requested_swapchain_height = 0;
    manager.m_last_observed_window_width = 0;
    manager.m_last_observed_window_height = 0;
    manager.m_resize_request_stable_frame_count = 0;
    manager.m_swapchain_resize_retry_countdown_frames = 0;
    manager.m_swapchain_resize_failure_count = 0;
    manager.m_swapchain_resize_last_failed_width = 0;
    manager.m_swapchain_resize_last_failed_height = 0;
    manager.m_swapchain_acquire_failure_count = 0;
    manager.m_swapchain_present_failure_count = 0;
    manager.SetSwapchainLifecycleState(
        manager.HasCurrentSwapchainRT() ? RendererInterface::SwapchainLifecycleState::RESIZE_PENDING
                                        : RendererInterface::SwapchainLifecycleState::INVALID,
        "invalidate resize request");
}

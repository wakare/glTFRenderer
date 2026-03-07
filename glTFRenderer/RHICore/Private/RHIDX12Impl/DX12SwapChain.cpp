#include "DX12SwapChain.h"

#include <GLFW/glfw3.h>

#include "DX12CommandQueue.h"
#include "DX12ConverterUtils.h"
#include "DX12Factory.h"
#include "DX12Utils.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RenderWindow/glTFWindow.h"

namespace
{
    RHIPresentFailureKind ClassifyDX12PresentFailure(HRESULT present_hr)
    {
        switch (present_hr)
        {
        case DXGI_ERROR_DEVICE_REMOVED:
        case DXGI_ERROR_DEVICE_RESET:
        case DXGI_ERROR_DEVICE_HUNG:
            return RHIPresentFailureKind::DEVICE_LOST;
        default:
            return RHIPresentFailureKind::UNKNOWN_FATAL;
        }
    }

    const char* ToString(RHIPresentFailureKind kind)
    {
        switch (kind)
        {
        case RHIPresentFailureKind::NONE:
            return "NONE";
        case RHIPresentFailureKind::RESIZE_REQUIRED:
            return "RESIZE_REQUIRED";
        case RHIPresentFailureKind::DEVICE_LOST:
            return "DEVICE_LOST";
        case RHIPresentFailureKind::UNKNOWN_FATAL:
        default:
            return "UNKNOWN_FATAL";
        }
    }

#if defined(__ID3D12DeviceRemovedExtendedData1_INTERFACE_DEFINED__)
    void LogDX12DREDDiagnostics(ID3D12Device* device)
    {
        if (!device)
        {
            return;
        }

        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))) || !dred)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] DRED interface unavailable on removed device.\n");
            return;
        }

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
        if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)))
        {
            if (!breadcrumbs.pHeadAutoBreadcrumbNode)
            {
                LOG_FORMAT_FLUSH("[DX12SwapChain] DRED auto-breadcrumbs empty.\n");
            }
            unsigned breadcrumb_index = 0;
            for (auto node = breadcrumbs.pHeadAutoBreadcrumbNode; node && breadcrumb_index < 4; node = node->pNext, ++breadcrumb_index)
            {
                const char* queue_name = node->pCommandQueueDebugNameA ? node->pCommandQueueDebugNameA : "<unnamed-queue>";
                const char* list_name = node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "<unnamed-command-list>";
                const UINT last_breadcrumb_value = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
                LOG_FORMAT_FLUSH("[DX12SwapChain] DRED breadcrumb[%u]: queue='%s' list='%s' last=%u/%u.\n",
                    breadcrumb_index,
                    queue_name,
                    list_name,
                    last_breadcrumb_value,
                    node->BreadcrumbCount);

                if (node->BreadcrumbContextsCount > 0 && node->pBreadcrumbContexts)
                {
                    const UINT first_context_index = node->BreadcrumbContextsCount > 4 ? node->BreadcrumbContextsCount - 4 : 0;
                    for (UINT context_index = first_context_index; context_index < node->BreadcrumbContextsCount; ++context_index)
                    {
                        const auto& context = node->pBreadcrumbContexts[context_index];
                        if (!context.pContextString)
                        {
                            continue;
                        }

                        LOG_FORMAT_FLUSH("[DX12SwapChain] DRED context[%u:%u]: %ls\n",
                            breadcrumb_index,
                            context.BreadcrumbIndex,
                            context.pContextString);
                    }
                }
            }
        }
        else
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] Failed to fetch DRED auto-breadcrumbs output.\n");
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT1 page_fault{};
        if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&page_fault)) && page_fault.PageFaultVA != 0)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] DRED page fault VA=0x%llX.\n",
                static_cast<unsigned long long>(page_fault.PageFaultVA));
        }
        else
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] DRED page fault output empty.\n");
        }
    }
#else
    void LogDX12DREDDiagnostics(ID3D12Device* device)
    {
        (void)device;
        LOG_FORMAT_FLUSH("[DX12SwapChain] DRED support not compiled in this SDK configuration.\n");
    }
#endif

    void LogDX12DeviceRemovedDiagnostics(IDXGISwapChain3& swap_chain)
    {
        ComPtr<ID3D12Device> device;
        const HRESULT get_device_hr = swap_chain.GetDevice(IID_PPV_ARGS(&device));
        if (FAILED(get_device_hr) || !device)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] Failed to query device for removal diagnostics (hr=0x%08X).\n", get_device_hr);
            return;
        }

        const HRESULT removed_reason = device->GetDeviceRemovedReason();
        LOG_FORMAT_FLUSH("[DX12SwapChain] Device removed reason=0x%08X.\n", removed_reason);
        LogDX12DREDDiagnostics(device.Get());
    }
}

unsigned DX12SwapChain::GetCurrentBackBufferIndex()
{
    return m_swap_chain ? m_swap_chain->GetCurrentBackBufferIndex() : 0;
}

unsigned DX12SwapChain::GetBackBufferCount()
{
    return m_frame_buffer_count;
}

UINT DX12SwapChain::GetSwapChainFlags() const
{
    UINT flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (m_swap_chain_mode == MAILBOX)
    {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
    return flags;
}

bool DX12SwapChain::WaitForFrameLatencyObject(unsigned timeout_ms) const
{
    if (!m_frame_latency_waitable_object)
    {
        return true;
    }

    const DWORD wait_result = WaitForSingleObject(m_frame_latency_waitable_object, timeout_ms);
    if (wait_result == WAIT_OBJECT_0)
    {
        return true;
    }
    if (wait_result == WAIT_TIMEOUT)
    {
        return false;
    }

    LOG_FORMAT_FLUSH("[DX12SwapChain] Wait frame latency object failed (result=%lu).\n", static_cast<unsigned long>(wait_result));
    return false;
}

bool DX12SwapChain::InitSwapChain(IRHIFactory& factory, IRHIDevice& device, IRHICommandQueue& command_queue, const RHITextureDesc& swap_chain_buffer_desc, const
                                  RHISwapChainDesc& swap_chain_desc)
{
    if (m_frame_latency_waitable_object)
    {
        CloseHandle(m_frame_latency_waitable_object);
        m_frame_latency_waitable_object = nullptr;
    }

    m_swap_chain_buffer_desc = swap_chain_buffer_desc;
    m_swap_chain_mode = swap_chain_desc.chain_mode;
    
    // -- Create the Swap Chain (double/tripple buffering) -- //
    DXGI_MODE_DESC back_buffer_desc = {}; // this is to describe our display mode
    back_buffer_desc.Width = GetWidth(); // buffer width
    back_buffer_desc.Height = GetHeight(); // buffer height
    back_buffer_desc.Format = DX12ConverterUtils::ConvertToDXGIFormat(GetBackBufferFormat()); // format of the buffer (rgba 32 bits, 8 bits for each chanel)
    
    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    m_swap_chain_sample_desc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
    
    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC dxgi_swap_chain_desc = {};
    dxgi_swap_chain_desc.BufferCount = m_frame_buffer_count; // number of buffers we have
    dxgi_swap_chain_desc.BufferDesc = back_buffer_desc; // our back buffer description
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_RENDER_TARGET)
    {
        dxgi_swap_chain_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    }
    if (swap_chain_buffer_desc.GetUsage() & RUF_ALLOW_UAV)
    {
        dxgi_swap_chain_desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS; // this says the pipeline will render to this swap chain
    }
    dxgi_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    dxgi_swap_chain_desc.OutputWindow = swap_chain_desc.hwnd; // handle to our window
    dxgi_swap_chain_desc.SampleDesc = m_swap_chain_sample_desc; // our multi-sampling description
    dxgi_swap_chain_desc.Windowed = !swap_chain_desc.full_screen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps
    dxgi_swap_chain_desc.Flags = GetSwapChainFlags();
    if (dxgi_swap_chain_desc.BufferUsage == 0)
    {
        dxgi_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    }

    auto dx_factory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    auto dx_command_queue = dynamic_cast<DX12CommandQueue&>(command_queue).GetCommandQueue();
    if (dx_factory == nullptr || dx_command_queue == nullptr || swap_chain_desc.hwnd == nullptr)
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] InitSwapChain failed: invalid factory/queue/window handle.\n");
        return false;
    }

    ComPtr<IDXGISwapChain> temp_swap_chain;
    const HRESULT create_hr = dx_factory->CreateSwapChain(
        dx_command_queue, // the queue will be flushed once the swap chain is created
        &dxgi_swap_chain_desc, // give it the swap chain description we created above
        &temp_swap_chain // store the created swap chain in a temp IDXGISwapChain interface
        );
    if (FAILED(create_hr) || !temp_swap_chain)
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] CreateSwapChain failed (hr=0x%08X) for %ux%u.\n", create_hr, back_buffer_desc.Width, back_buffer_desc.Height);
        return false;
    }

    const HRESULT as_hr = temp_swap_chain.As(&m_swap_chain);
    if (FAILED(as_hr) || !m_swap_chain)
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] Query IDXGISwapChain3 failed (hr=0x%08X).\n", as_hr);
        return false;
    }

    ComPtr<IDXGISwapChain2> swap_chain2;
    const HRESULT as_swapchain2_hr = m_swap_chain.As(&swap_chain2);
    if (SUCCEEDED(as_swapchain2_hr) && swap_chain2)
    {
        const UINT maximum_frame_latency = m_frame_buffer_count > 0 ? m_frame_buffer_count : 1;
        const HRESULT frame_latency_hr = swap_chain2->SetMaximumFrameLatency(maximum_frame_latency);
        if (FAILED(frame_latency_hr))
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] SetMaximumFrameLatency(%u) failed (hr=0x%08X).\n",
                maximum_frame_latency,
                frame_latency_hr);
        }
        m_frame_latency_waitable_object = swap_chain2->GetFrameLatencyWaitableObject();
        if (!m_frame_latency_waitable_object)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] GetFrameLatencyWaitableObject returned null.\n");
        }
    }
    else
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] Query IDXGISwapChain2 failed (hr=0x%08X).\n", as_swapchain2_hr);
    }

    m_fence = RHIResourceFactory::CreateRHIResource<IRHIFence>();
    if (!m_fence || !m_fence->InitFence(device))
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] Init fence failed.\n");
        m_fence.reset();
        if (m_frame_latency_waitable_object)
        {
            CloseHandle(m_frame_latency_waitable_object);
            m_frame_latency_waitable_object = nullptr;
        }
        m_swap_chain.Reset();
        return false;
    }
    
    need_release = true;
    
    return true;
}

bool DX12SwapChain::AcquireNewFrame(IRHIDevice& device)
{
    (void)device;
    return WaitForFrameLatencyObject(INFINITE);
}

IRHISemaphore& DX12SwapChain::GetAvailableFrameSemaphore()
{
    static RHISemaphoreNull _dummy_semaphore;
    return _dummy_semaphore;
}

bool DX12SwapChain::Present(IRHICommandQueue& command_queue, IRHICommandList& command_list)
{
    if (!m_swap_chain || !m_fence)
    {
        return false;
    }
    auto sync_interval = m_swap_chain_mode == VSYNC ? 1 : 0;
    UINT flag = m_swap_chain_mode == VSYNC ? 0 : DXGI_PRESENT_ALLOW_TEARING ;
    const HRESULT present_hr = m_swap_chain->Present(sync_interval, flag);
    if (FAILED(present_hr))
    {
        m_last_present_hr = present_hr;
        m_last_present_failure_kind = ClassifyDX12PresentFailure(present_hr);
        LOG_FORMAT_FLUSH("[DX12SwapChain] Present failed (hr=0x%08X, kind=%s).\n",
            present_hr,
            ToString(m_last_present_failure_kind));
        if (m_last_present_failure_kind == RHIPresentFailureKind::DEVICE_LOST)
        {
            LogDX12DeviceRemovedDiagnostics(*m_swap_chain.Get());
        }
        return false;
    }

    m_last_present_hr = S_OK;
    m_last_present_failure_kind = RHIPresentFailureKind::NONE;
    dynamic_cast<DX12Fence&>(*m_fence).SignalWhenCommandQueueFinish(command_queue);
    
    return true;
}

bool DX12SwapChain::HostWaitPresentFinished(IRHIDevice& device)
{
    (void)device;
    if (!m_fence)
    {
        return true;
    }
    m_fence->HostWaitUtilSignaled();
    m_fence->ResetFence();
    
    return true;
}

bool DX12SwapChain::ResizeSwapChain(unsigned width, unsigned height)
{
    if (!m_swap_chain || width == 0 || height == 0)
    {
        return false;
    }
    if (GetWidth() == width && GetHeight() == height)
    {
        return true;
    }

    const DXGI_FORMAT back_buffer_format = DX12ConverterUtils::ConvertToDXGIFormat(GetBackBufferFormat());
    if (!WaitForFrameLatencyObject(0))
    {
        ++m_resize_failure_count;
        const bool should_log =
            m_resize_failure_count == 1 ||
            m_last_resize_failed_width != width ||
            m_last_resize_failed_height != height ||
            (m_resize_failure_count % 30) == 0;
        if (should_log)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] Resize deferred (frame latency not ready) for %ux%u (count=%u).\n",
                width, height, m_resize_failure_count);
        }
        m_last_resize_failed_width = width;
        m_last_resize_failed_height = height;
        m_last_resize_failed_hr = DXGI_ERROR_WAS_STILL_DRAWING;
        return false;
    }

    const UINT flags = GetSwapChainFlags();

    const HRESULT resize_hr = m_swap_chain->ResizeBuffers(m_frame_buffer_count, width, height, back_buffer_format, flags);
    if (FAILED(resize_hr))
    {
        ++m_resize_failure_count;
        const bool should_log =
            m_resize_failure_count == 1 ||
            m_last_resize_failed_width != width ||
            m_last_resize_failed_height != height ||
            m_last_resize_failed_hr != resize_hr ||
            (m_resize_failure_count % 30) == 0;
        if (should_log)
        {
            LOG_FORMAT_FLUSH("[DX12SwapChain] ResizeBuffers failed (hr=0x%08X) to %ux%u (count=%u).\n",
                resize_hr, width, height, m_resize_failure_count);
        }
        m_last_resize_failed_width = width;
        m_last_resize_failed_height = height;
        m_last_resize_failed_hr = resize_hr;
        return false;
    }

    if (m_resize_failure_count > 0)
    {
        LOG_FORMAT_FLUSH("[DX12SwapChain] ResizeBuffers recovered after %u retries.\n", m_resize_failure_count);
    }
    m_resize_failure_count = 0;
    m_last_resize_failed_width = 0;
    m_last_resize_failed_height = 0;
    m_last_resize_failed_hr = S_OK;

    m_swap_chain_buffer_desc = RHITextureDesc(
        m_swap_chain_buffer_desc.GetName(),
        width,
        height,
        m_swap_chain_buffer_desc.GetDataFormat(),
        m_swap_chain_buffer_desc.GetUsage(),
        m_swap_chain_buffer_desc.GetClearValue());
    LOG_FORMAT_FLUSH("[DX12SwapChain] ResizeBuffers succeeded to %ux%u.\n", width, height);
    return true;
}

bool DX12SwapChain::Release(IRHIMemoryManager& memory_manager)
{
    if (m_fence)
    {
        m_fence->Release(memory_manager);
        m_fence.reset();
    }
    if (m_frame_latency_waitable_object)
    {
        CloseHandle(m_frame_latency_waitable_object);
        m_frame_latency_waitable_object = nullptr;
    }
    SAFE_RELEASE(m_swap_chain)
    need_release = false;
    return true;
}

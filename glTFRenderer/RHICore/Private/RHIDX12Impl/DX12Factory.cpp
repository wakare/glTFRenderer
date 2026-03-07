#include "DX12Factory.h"

#include <d3d12.h>

#include "DX12Utils.h"
#include "RHIConfigSingleton.h"
#include "RenderWindow/glTFWindow.h"

namespace
{
    bool EnableDX12DeviceRemovedDiagnostics()
    {
#if defined(__ID3D12DeviceRemovedExtendedDataSettings1_INTERFACE_DEFINED__)
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings1;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred_settings1))) && dred_settings1)
        {
            dred_settings1->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings1->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings1->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings1->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            return true;
        }
#endif

#if defined(__ID3D12DeviceRemovedExtendedDataSettings_INTERFACE_DEFINED__)
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred_settings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred_settings))) && dred_settings)
        {
            dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            return true;
        }
#endif

        return false;
    }
}

bool DX12Factory::InitFactory()
{
    const auto& config = RHIConfigSingleton::Instance();
    const bool enable_api_validation = config.IsAPIValidationEnabled();
    const bool dred_enabled = EnableDX12DeviceRemovedDiagnostics();
    LOG_FORMAT_FLUSH("[DX12Factory] Device removed diagnostics %s.\n", dred_enabled ? "enabled" : "unavailable");
    if (enable_api_validation)
    {
        LOG_FORMAT_FLUSH("[DX12Factory] API validation enabled. GPU validation=%s, sync queue validation=%s.\n",
            config.IsDX12GPUBasedValidationEnabled() ? "on" : "off",
            config.IsDX12SynchronizedQueueValidationEnabled() ? "on" : "off");
    }
    if (enable_api_validation && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_controller))))
    {
        m_debug_controller->EnableDebugLayer();
        ComPtr<ID3D12Debug1> debug;
        if (SUCCEEDED(m_debug_controller->QueryInterface(IID_PPV_ARGS(&debug))))
        {
            if (config.IsDX12GPUBasedValidationEnabled())
            {
                debug->SetEnableGPUBasedValidation(true);
            }
            if (config.IsDX12SynchronizedQueueValidationEnabled())
            {
                debug->SetEnableSynchronizedCommandQueueValidation(true);
            }
        }
    }
    
    // -- Create the Device -- //
    const UINT factory_flags = enable_api_validation ? DXGI_CREATE_FACTORY_DEBUG : 0;
    HRESULT hr = CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
    {
        return false;
    }

    need_release = true;
    
    return true;
}

bool DX12Factory::Release(IRHIMemoryManager& memory_manager)
{
    m_factory->MakeWindowAssociation(glTFWindow::Get().GetHWND(), DXGI_MWA_NO_ALT_ENTER);

    SAFE_RELEASE(m_factory)
    SAFE_RELEASE(m_debug_controller)
    
    return true;
}

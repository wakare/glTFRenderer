#include "DX12Factory.h"

#include <d3d12.h>

#include "DX12Utils.h"
#include "RHIConfigSingleton.h"
#include "RenderWindow/glTFWindow.h"

bool DX12Factory::InitFactory()
{
    const auto& config = RHIConfigSingleton::Instance();
    const bool enable_api_validation = config.IsAPIValidationEnabled();
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

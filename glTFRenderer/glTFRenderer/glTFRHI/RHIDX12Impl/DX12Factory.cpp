#include "DX12Factory.h"

#include <d3d12.h>

#include "DX12Utils.h"

bool DX12Factory::InitFactory()
{
    if SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_controller)))
    {
        m_debug_controller->EnableDebugLayer();
        ComPtr<ID3D12Debug1> debug;
        if (SUCCEEDED(m_debug_controller->QueryInterface(IID_PPV_ARGS(&debug))))
        {
            debug->SetEnableGPUBasedValidation(true);
            debug->SetEnableSynchronizedCommandQueueValidation(true);
        }
    }
    
    // -- Create the Device -- //
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
    {
        return false;
    }

    need_release = true;
    
    return true;
}

bool DX12Factory::Release(glTFRenderResourceManager&)
{
    SAFE_RELEASE(m_factory)
    SAFE_RELEASE(m_debug_controller)
    
    return true;
}

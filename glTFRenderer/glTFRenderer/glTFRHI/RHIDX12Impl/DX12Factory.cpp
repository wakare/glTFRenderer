#include "DX12Factory.h"

#include <d3d12.h>

#include "DX12Utils.h"

bool DX12Factory::InitFactory()
{
    ID3D12Debug* debug_controller = nullptr;
    THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))
    debug_controller->EnableDebugLayer();
    
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
    
    return true;
}

#include "DX12Factory.h"

#include <d3d12.h>

#include "DX12Utils.h"

DX12Factory::DX12Factory() = default;

DX12Factory::~DX12Factory()
{
    SAFE_RELEASE(m_factory)
}

bool DX12Factory::InitFactory()
{
    ID3D12Debug* debugController = nullptr;
    THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))
    debugController->EnableDebugLayer();
    
    // -- Create the Device -- //
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}

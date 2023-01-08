#include "DX12Device.h"

#include <dxgi.h>

#include "DX12Factory.h"

DX12Device::DX12Device()
    : m_device(nullptr)
{
    
}

bool DX12Device::InitDevice(IRHIFactory& factory)
{
    IDXGIAdapter1* adapter; // adapters are the graphics card (this includes the embedded graphics on the motherboard)

    int adapterIndex = 0; // we'll start looking for directx 12  compatible graphics devices starting at index 0

    bool adapterFound = false; // set this to true when a good one was found

    // find first hardware gpu that supports d3d 12
    auto dxFactory = dynamic_cast<DX12Factory&>(factory).GetFactory();
    if (!dxFactory)
    {
        return false;
    }
    
    while (dxFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // we dont want a software device
            adapterIndex++; // add this line here. Its not currently in the downloadable project
            continue;
        }

        // we want a device that is compatible with direct3d 12 (feature level 11 or higher)
        HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {
            adapterFound = true;
            break;
        }

        adapterIndex++;
    }

    if (!adapterFound)
    {
        return false;
    }

    // Create the device
    HRESULT hr = D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
        );
    
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}

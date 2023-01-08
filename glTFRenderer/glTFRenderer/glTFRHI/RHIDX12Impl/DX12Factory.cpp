#include "DX12Factory.h"

DX12Factory::DX12Factory()
    : m_factory(nullptr)
{
    
}

bool DX12Factory::InitFactory()
{
    // -- Create the Device -- //
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
    {
        return false;
    }
    
    return true;
}

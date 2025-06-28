#pragma once

#include <dxgi.h>

#include "RHIInterface/IRHIDevice.h"
#include "DX12Common.h"

class RHICORE_API DX12Device : public IRHIDevice
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Device)

    virtual bool InitDevice(IRHIFactory& factory) override;
    
    ID3D12Device* GetDevice() { return m_device.Get();}
    const ID3D12Device* GetDevice() const {return m_device.Get();}

    ID3D12Device5* GetDXRDevice() { return m_dxr_device.Get(); }
    const ID3D12Device5* GetDXRDevice() const { return m_dxr_device.Get(); }
    
    IDXGIAdapter1* GetAdapter() {return m_adapter.Get(); }
    const IDXGIAdapter1* GetAdapter() const {return m_adapter.Get(); }

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
private:
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIAdapter1> m_adapter;
    ComPtr<ID3D12Device5> m_dxr_device;
};

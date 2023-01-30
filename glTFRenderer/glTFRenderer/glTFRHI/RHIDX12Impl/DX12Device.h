#pragma once
#include "../RHIInterface/IRHIDevice.h"
#include <d3d12.h>

class DX12Device : public IRHIDevice
{
public:
    DX12Device();
    virtual ~DX12Device() override;

    virtual bool InitDevice(IRHIFactory& factory) override;
    
    ID3D12Device* GetDevice() { return m_device;}
    const ID3D12Device* GetDevice() const {return m_device;}
    
private:
    ID3D12Device* m_device;
};

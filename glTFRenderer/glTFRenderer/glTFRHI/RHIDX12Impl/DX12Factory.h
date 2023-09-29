#pragma once
#include <dxgi1_4.h>
#include "DX12Common.h"
#include "../RHIInterface/IRHIFactory.h"

class DX12Factory : public IRHIFactory
{
public:
    DX12Factory();
    virtual ~DX12Factory() override;
    
    virtual bool InitFactory() override;

    IDXGIFactory4* GetFactory() {return m_factory.Get();}
    const IDXGIFactory4* GetFactory() const {return m_factory.Get();}
    
private:
    ComPtr<IDXGIFactory4> m_factory;
};

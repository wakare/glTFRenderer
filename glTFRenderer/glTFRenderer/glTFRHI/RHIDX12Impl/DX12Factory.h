#pragma once
#include "DX12Common.h"
#include "../RHIInterface/IRHIFactory.h"

class DX12Factory : public IRHIFactory
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Factory)
    
    virtual bool InitFactory() override;

    IDXGIFactory4* GetFactory() {return m_factory.Get();}
    const IDXGIFactory4* GetFactory() const {return m_factory.Get();}

    virtual bool Release(glTFRenderResourceManager&) override;
    
private:
    ComPtr<IDXGIFactory4> m_factory;
};

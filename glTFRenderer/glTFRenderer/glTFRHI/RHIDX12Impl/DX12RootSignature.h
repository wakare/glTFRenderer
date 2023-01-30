#pragma once
#include <d3d12.h>

#include "../RHIInterface/IRHIRootSignature.h"

class DX12RootParameter : public IRHIRootParameter
{
public:
    DX12RootParameter();
    virtual ~DX12RootParameter() override;

    virtual bool InitAsConstant(unsigned constantValue, REGISTER_INDEX_TYPE registerIndex) override;
    virtual bool InitAsCBV(REGISTER_INDEX_TYPE registerIndex) override;
    virtual bool InitAsSRV(REGISTER_INDEX_TYPE registerIndex) override;
    virtual bool InitAsUAV(REGISTER_INDEX_TYPE registerIndex) override;
    virtual bool InitAsDescriptorTableRange(size_t rangeCount, const RHIRootParameterDescriptorRangeDesc* rangeDesc) override;

    const D3D12_ROOT_PARAMETER& GetParameter() const {return m_parameter;}
    const std::vector<D3D12_DESCRIPTOR_RANGE>& GetRanges() const {return m_ranges;}
    
private:
    D3D12_ROOT_PARAMETER m_parameter;
    std::vector<D3D12_DESCRIPTOR_RANGE> m_ranges;
};

class DX12StaticSampler : public IRHIStaticSampler
{
public:
    DX12StaticSampler();
    virtual bool InitStaticSampler(REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode) override;

    const D3D12_STATIC_SAMPLER_DESC& GetStaticSamplerDesc() const {return m_description;}
    
private:
    D3D12_STATIC_SAMPLER_DESC m_description;
};

class DX12RootSignature : public IRHIRootSignature
{
public:
    DX12RootSignature();
    
    virtual bool InitRootSignature(IRHIDevice& device) override;
    ID3D12RootSignature* GetRootSignature() const;
    
private:
    ID3D12RootSignature* m_rootSignature;
    D3D12_ROOT_SIGNATURE_DESC m_description;
};

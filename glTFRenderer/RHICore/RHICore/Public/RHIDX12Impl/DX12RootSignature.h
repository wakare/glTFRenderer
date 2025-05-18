#pragma once
#include "DX12Common.h"

#include "IRHIRootSignature.h"

class DX12RootParameter : public IRHIRootParameter
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12RootParameter)

    virtual bool InitAsConstant(unsigned constant_value_count, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsCBV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsSRV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsUAV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) override;
    virtual bool InitAsDescriptorTableRange(unsigned attribute_index, size_t rangeCount, const RHIDescriptorRangeDesc* range_desc) override;

    const D3D12_ROOT_PARAMETER1& GetParameter() const {return m_parameter;}
    const std::vector<D3D12_DESCRIPTOR_RANGE1>& GetRanges() const {return m_ranges;}
    
private:
    D3D12_ROOT_PARAMETER1 m_parameter{};
    std::vector<D3D12_DESCRIPTOR_RANGE1> m_ranges;
};

class DX12StaticSampler : public IRHIStaticSampler
{
public:
    DX12StaticSampler();
    virtual bool InitStaticSampler(IRHIDevice& device, unsigned space, REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode) override;
    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
    const D3D12_STATIC_SAMPLER_DESC& GetStaticSamplerDesc() const {return m_description;}
    
private:
    D3D12_STATIC_SAMPLER_DESC m_description;
};

class DX12RootSignature : public IRHIRootSignature
{
public:
    DX12RootSignature();
    
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) override;
    ID3D12RootSignature* GetRootSignature() const;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
private:
    ComPtr<ID3D12RootSignature> m_root_signature;
};

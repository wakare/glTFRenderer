#pragma once
#include <cassert>
#include <memory>
#include <vector>

#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIDescriptorManager;

class IRHIRootParameter
{
public:
    IRHIRootParameter() : m_type(RHIRootParameterType::Unknown) { }
    DECLARE_NON_COPYABLE_AND_VDTOR(IRHIRootParameter)
    
    virtual bool InitAsConstant(unsigned constantValue, REGISTER_INDEX_TYPE register_index, unsigned space) = 0;
    virtual bool InitAsCBV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) = 0;
    virtual bool InitAsSRV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) = 0;
    virtual bool InitAsUAV(unsigned attribute_index, REGISTER_INDEX_TYPE register_index, unsigned space) = 0;
    virtual bool InitAsDescriptorTableRange(unsigned attribute_index, size_t rangeCount, const RHIDescriptorRangeDesc* rangeDesc) = 0;

    virtual bool IsBindless() const {return false; }
    RHIRootParameterType GetType() const { return m_type; }
    
protected:
    void SetType(RHIRootParameterType type) {assert(m_type == RHIRootParameterType::Unknown); m_type = type;}
    
    RHIRootParameterType m_type;
};

class IRHIStaticSampler : public IRHIResource
{
public:
    IRHIStaticSampler()
        : m_registerIndex(0)
        , m_addressMode(RHIStaticSamplerAddressMode::Unknown)
        , m_filterMode(RHIStaticSamplerFilterMode::Unknown)
    {
    }

    virtual bool InitStaticSampler(IRHIDevice& device, unsigned space, REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode) = 0;
    
protected:
    REGISTER_INDEX_TYPE m_registerIndex;
    RHIStaticSamplerAddressMode m_addressMode;
    RHIStaticSamplerFilterMode m_filterMode;
};

class IRHIRootSignature : public IRHIResource
{
public:
    IRHIRootSignature();
    
    bool AllocateRootSignatureSpace(size_t rootParameterCount, size_t staticSamplerCount);
    void SetUsage (RHIRootSignatureUsage usage) { m_usage = usage; }
    bool IsSpaceAllocated() const {return !m_root_parameters.empty() || !m_static_samplers.empty(); }
    bool HasSampler() const;
    
    virtual bool InitRootSignature(IRHIDevice& device, IRHIDescriptorManager& descriptor_manager) = 0;
    
    IRHIRootParameter& GetRootParameter(size_t index) {return *m_root_parameters[index];}
    const IRHIRootParameter& GetRootParameter(size_t index) const {return *m_root_parameters[index];}

    IRHIStaticSampler& GetStaticSampler(size_t index) {return *m_static_samplers[index];}
    const IRHIStaticSampler& GetStaticSampler(size_t index) const {return *m_static_samplers[index];}
    
protected:
    std::vector<std::shared_ptr<IRHIRootParameter>> m_root_parameters;
    std::vector<std::shared_ptr<IRHIStaticSampler>> m_static_samplers;

    RHIRootSignatureUsage m_usage;
};

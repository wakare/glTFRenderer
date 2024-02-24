#pragma once
#include <cassert>
#include <memory>
#include <vector>

#include "IRHIDevice.h"
#include "IRHIResource.h"

class IRHIRootParameter : public IRHIResource
{
public:
    IRHIRootParameter() : m_type(RHIRootParameterType::Unknown) { }
    virtual bool InitAsConstant(unsigned constantValue, REGISTER_INDEX_TYPE register_index, unsigned space) = 0;
    virtual bool InitAsCBV(REGISTER_INDEX_TYPE registerIndex, unsigned space) = 0;
    virtual bool InitAsSRV(REGISTER_INDEX_TYPE registerIndex, unsigned space) = 0;
    virtual bool InitAsUAV(REGISTER_INDEX_TYPE registerIndex, unsigned space) = 0;
    virtual bool InitAsDescriptorTableRange(size_t rangeCount, const RHIRootParameterDescriptorRangeDesc* rangeDesc) = 0;
    
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

    virtual bool InitStaticSampler(REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode) = 0;
    
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
    bool IsSpaceAllocated() const {return !m_rootParameters.empty() || !m_staticSampler.empty(); }
    
    virtual bool InitRootSignature(IRHIDevice& device) = 0;
    
    IRHIRootParameter& GetRootParameter(size_t index) {return *m_rootParameters[index];}
    const IRHIRootParameter& GetRootParameter(size_t index) const {return *m_rootParameters[index];}

    IRHIStaticSampler& GetStaticSampler(size_t index) {return *m_staticSampler[index];}
    const IRHIStaticSampler& GetStaticSampler(size_t index) const {return *m_staticSampler[index];}
    
protected:
    std::vector<std::shared_ptr<IRHIRootParameter>> m_rootParameters;
    std::vector<std::shared_ptr<IRHIStaticSampler>> m_staticSampler;

    RHIRootSignatureUsage m_usage;
};

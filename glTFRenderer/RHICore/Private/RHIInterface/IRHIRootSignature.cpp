#include "RHIInterface/IRHIRootSignature.h"

#include "../RHIResourceFactoryImpl.hpp"

bool IRHIRootParameter::IsBindless() const
{
    return false;
}

RHIRootParameterType IRHIRootParameter::GetType() const
{
    return m_type;
}

void IRHIRootParameter::SetType(RHIRootParameterType type)
{
    assert(m_type == RHIRootParameterType::Unknown); m_type = type;
}

IRHIRootSignature::IRHIRootSignature()
    : m_usage(RHIRootSignatureUsage::None)
{
}

bool IRHIRootSignature::AllocateRootSignatureSpace(size_t rootParameterCount, size_t staticSamplerCount)
{
    // Allocate once now
    if (IsSpaceAllocated())
    {
        return false;
    }
        
    m_root_parameters.resize(rootParameterCount);
    m_static_samplers.resize(staticSamplerCount);

    for (size_t i = 0; i < rootParameterCount; ++i)
    {
        m_root_parameters[i] = RHIResourceFactory::CreateRHIResource<IRHIRootParameter>();
    }

    for (size_t i = 0; i < staticSamplerCount; ++i)
    {
        m_static_samplers[i] = RHIResourceFactory::CreateRHIResource<IRHIStaticSampler>();
    }
        
    return true;
}

void IRHIRootSignature::SetUsage(RHIRootSignatureUsage usage)
{
    m_usage = usage;
}

bool IRHIRootSignature::IsSpaceAllocated() const
{
    return !m_root_parameters.empty() || !m_static_samplers.empty();
}

bool IRHIRootSignature::HasSampler() const
{
    return !m_static_samplers.empty();
}

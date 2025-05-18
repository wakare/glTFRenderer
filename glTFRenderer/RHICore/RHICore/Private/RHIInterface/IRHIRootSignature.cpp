#include "IRHIRootSignature.h"

#include "../RHIResourceFactoryImpl.hpp"

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

bool IRHIRootSignature::HasSampler() const
{
    return !m_static_samplers.empty();
}

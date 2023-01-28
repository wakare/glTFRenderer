#include "IRHIRootSignature.h"

#include "../RHIResourceFactoryImpl.hpp"

bool IRHIRootSignature::AllocateRootSignatureSpace(size_t rootParameterCount, size_t staticSamplerCount)
{
    // Allocate once now
    if (IsSpaceAllocated())
    {
        return false;
    }
        
    m_rootParameters.resize(rootParameterCount);
    m_staticSampler.resize(staticSamplerCount);

    for (size_t i = 0; i < rootParameterCount; ++i)
    {
        m_rootParameters[i] = RHIResourceFactory::CreateRHIResource<IRHIRootParameter>();
    }

    for (size_t i = 0; i < staticSamplerCount; ++i)
    {
        m_staticSampler[i] = RHIResourceFactory::CreateRHIResource<IRHIStaticSampler>();
    }
        
    return true;
}

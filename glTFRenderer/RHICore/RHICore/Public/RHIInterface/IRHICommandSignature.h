#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"
#include "IRHIRootSignature.h"

class RHICORE_API IRHICommandSignature : public IRHIResource
{
public:
    IRHICommandSignature();
    
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) = 0;
    
    void SetCommandSignatureDesc(const RHICommandSignatureDesc& desc);
    
protected:
    RHICommandSignatureDesc m_desc;
};

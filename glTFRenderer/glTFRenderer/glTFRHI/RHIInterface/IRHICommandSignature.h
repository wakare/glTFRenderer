#pragma once
#include "IRHIDevice.h"
#include "IRHIResource.h"
#include "IRHIRootSignature.h"

class IRHICommandSignature : public IRHIResource
{
public:
    IRHICommandSignature();
    
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) = 0;
    
    void SetCommandSignatureDesc(const IRHICommandSignatureDesc& desc);
    
protected:
    IRHICommandSignatureDesc m_desc;
};

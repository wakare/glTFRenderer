#pragma once
#include "RHIInterface/IRHICommandSignature.h"

class VKCommandSignature : public IRHICommandSignature
{
public:
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) override;

    virtual bool Release(IRHIMemoryManager& memory_manager) override;
    
};

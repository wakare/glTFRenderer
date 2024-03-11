#pragma once
#include "glTFRHI/RHIInterface/IRHICommandSignature.h"

class VKCommandSignature : public IRHICommandSignature
{
public:
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) override;
};

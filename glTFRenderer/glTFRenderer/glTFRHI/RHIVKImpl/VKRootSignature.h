#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"

class VKRootSignature : public IRHIRootSignature
{
public:
    virtual bool InitRootSignature(IRHIDevice& device) override;
};

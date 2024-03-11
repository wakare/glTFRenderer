#pragma once
#include "glTFRHI/RHIInterface/IRHIRootSignature.h"

class VKStaticSampler : public IRHIStaticSampler
{
public:
    virtual bool InitStaticSampler(REGISTER_INDEX_TYPE registerIndex, RHIStaticSamplerAddressMode addressMode, RHIStaticSamplerFilterMode filterMode) override;
};

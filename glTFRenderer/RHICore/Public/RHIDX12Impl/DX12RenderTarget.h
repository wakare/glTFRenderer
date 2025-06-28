#pragma once
#include "RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();
    virtual ~DX12RenderTarget() override;
};
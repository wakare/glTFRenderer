#pragma once
#include "DX12Common.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"

class DX12RenderTarget : public IRHIRenderTarget
{
public:
    DX12RenderTarget();
    virtual ~DX12RenderTarget() override;
};
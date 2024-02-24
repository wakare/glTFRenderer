#pragma once
#include <d3dcommon.h>
#include <vector>

#include "../RHIInterface/IRHIShader.h"

class DX12Shader : public IRHIShader
{
protected:
    virtual bool CompileSpirV() override;
};

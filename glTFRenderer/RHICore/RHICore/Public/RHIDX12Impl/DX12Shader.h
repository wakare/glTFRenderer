#pragma once
#include "ShaderUtil/IRHIShader.h"

class DX12Shader : public IRHIShader
{
protected:
    virtual bool CompileSpirV() override;
};

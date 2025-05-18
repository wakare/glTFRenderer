#pragma once
#include "ShaderUtil/IRHIShader.h"

class VKShader : public IRHIShader
{
public:
    virtual bool CompileSpirV() override;
};

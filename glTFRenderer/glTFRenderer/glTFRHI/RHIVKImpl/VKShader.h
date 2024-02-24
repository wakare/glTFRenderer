#pragma once
#include "glTFRHI/RHIInterface/IRHIShader.h"

class VKShader : public IRHIShader
{
public:
    virtual bool CompileSpirV() override;
};

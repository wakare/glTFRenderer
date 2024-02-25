#include "IRHICommandSignature.h"

IRHICommandSignature::IRHICommandSignature()
= default;

void IRHICommandSignature::SetCommandSignatureDesc(const RHICommandSignatureDesc& desc)
{
    m_desc = desc;
}

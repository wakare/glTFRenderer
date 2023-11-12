#include "IRHICommandSignature.h"

IRHICommandSignature::IRHICommandSignature()
= default;

void IRHICommandSignature::SetCommandSignatureDesc(const IRHICommandSignatureDesc& desc)
{
    m_desc = desc;
}

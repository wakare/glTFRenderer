#include "IRHISwapChain.h"

#ifdef RHICORE_EXPORTS
#pragma message("✅ RHICORE_EXPORTS is defined in this file")
#else
#pragma message("❌ RHICORE_EXPORTS is NOT defined in this file")
#endif

unsigned IRHISwapChain::GetWidth() const
{
    return GetSwapChainBufferDesc().GetTextureWidth();
}

unsigned IRHISwapChain::GetHeight() const
{
    return GetSwapChainBufferDesc().GetTextureHeight();
}

RHIDataFormat IRHISwapChain::GetBackBufferFormat() const
{
    return GetSwapChainBufferDesc().GetDataFormat();
}

const RHITextureDesc& IRHISwapChain::GetSwapChainBufferDesc() const
{
    return m_swap_chain_buffer_desc;
}

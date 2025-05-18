#include "IRHISwapChain.h"

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

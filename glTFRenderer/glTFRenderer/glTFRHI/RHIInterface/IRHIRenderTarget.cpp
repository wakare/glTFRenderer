#include "IRHIRenderTarget.h"

std::atomic<RTID> IRHIRenderTarget::g_renderTargetId{0};

IRHIRenderTarget::IRHIRenderTarget()
    : m_id(g_renderTargetId++)
    , m_type(RHIRenderTargetType::Unknown)
    , m_format(RHIDataFormat::Unknown)
{
}

void IRHIRenderTarget::SetRenderTargetType(RHIRenderTargetType type)
{
    assert(m_type == RHIRenderTargetType::Unknown && type != RHIRenderTargetType::Unknown);
    m_type = type;
}

void IRHIRenderTarget::SetRenderTargetFormat(RHIDataFormat format)
{
    assert(m_format == RHIDataFormat::Unknown && format != RHIDataFormat::Unknown);
    m_format = format;
}

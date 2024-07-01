#pragma once
#include <atomic>

#include "IRHIResource.h"

class IRHIRenderTargetManager;

class IRHIRenderTarget : public IRHIResource
{
public:
    IRHIRenderTarget();
    virtual ~IRHIRenderTarget() override = default;
    DECLARE_NON_COPYABLE(IRHIRenderTarget)

    unsigned GetRenderTargetId() const {return m_id;}
    RHIRenderTargetType GetRenderTargetType() const { return m_type; }
    RHIDataFormat GetRenderTargetFormat() const { return m_format; }
    
    void SetRenderTargetType(RHIRenderTargetType type);
    void SetRenderTargetFormat(RHIDataFormat format);

private:
    RTID m_id;
    RHIRenderTargetType m_type;
    RHIDataFormat m_format;
    
    // Construct new id in ctor
    static std::atomic<RTID> g_renderTargetId;
};
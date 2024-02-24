#pragma once
#include <cassert>
#include <atomic>
#include <memory>
#include <string>
#include <vec4.hpp>

#include "IRHIGPUBuffer.h"
#include "IRHIResource.h"
#include "glTFUtils/glTFUtils.h"

class IRHIRenderTargetManager;

class IRHIRenderTarget : public IRHIResource
{
public:
    IRHIRenderTarget()
        : m_id(g_renderTargetId++)
        , m_type(RHIRenderTargetType::Unknown)
        , m_format(RHIDataFormat::Unknown)
    {
    }

    unsigned GetRenderTargetId() const {return m_id;}
    
    void SetRenderTargetType(RHIRenderTargetType type)
    {
        assert(m_type == RHIRenderTargetType::Unknown && type != RHIRenderTargetType::Unknown);
        m_type = type;
    }
    RHIRenderTargetType GetRenderTargetType() const { return m_type; }

    void SetRenderTargetFormat(RHIDataFormat format)
    {
        assert(m_format == RHIDataFormat::Unknown && format != RHIDataFormat::Unknown);
        m_format = format;
    }

    RHIDataFormat GetRenderTargetFormat() const { return m_format; }

private:
    RTID m_id;
    RHIRenderTargetType m_type;
    RHIDataFormat m_format;
    
    // Construct new id in ctor
    static std::atomic<RTID> g_renderTargetId;
};
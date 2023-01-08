#pragma once
#include <cassert>
#include <atomic>
#include <string>

#include "../IRHIResource.h"

struct IRHIDepthStencilClearValue
{
    float clearDepth;
    unsigned char clearStencilValue;
};

struct IRHIRenderTargetClearValue
{
    union 
    {
        float clearColor[4];
        IRHIDepthStencilClearValue clearDS;
    };
};

struct IRHIRenderTargetDesc
{
    unsigned width;
    unsigned height;
    bool isUAV;
    IRHIRenderTargetClearValue clearValue;
    std::string name;
};

enum class RHIRenderTargetType
{
    RTV,
    DSV,
    Unknown,
};

enum class RHIRenderTargetFormat
{
    R8G8B8A8_UNORM,
    R8G8B8A8_UNORM_SRGB,
    Unknown,
};

typedef unsigned RTID; 

class IRHIRenderTargetManager;

class IRHIRenderTarget : public IRHIResource
{
public:
    IRHIRenderTarget()
        : m_id(g_renderTargetId++)
        , m_type(RHIRenderTargetType::Unknown)
        , m_format(RHIRenderTargetFormat::Unknown)
    {
    }

    unsigned GetRenderTargetId() const {return m_id;}
    
    void SetRenderTargetType(RHIRenderTargetType type)
    {
        assert(m_type == RHIRenderTargetType::Unknown && type != RHIRenderTargetType::Unknown);
        m_type = type;
    }
    RHIRenderTargetType GetRenderTargetType() const { return m_type; }

    void SetRenderTargetFormat(RHIRenderTargetFormat format)
    {
        assert(m_format == RHIRenderTargetFormat::Unknown && format != RHIRenderTargetFormat::Unknown);
        m_format = format;
    }

    RHIRenderTargetFormat GetRenderTargetFormat() const { return m_format; }
    
private:
    RTID m_id;
    RHIRenderTargetType m_type;
    RHIRenderTargetFormat m_format;
    
    // Construct new id in ctor
    static std::atomic<RTID> g_renderTargetId;
};
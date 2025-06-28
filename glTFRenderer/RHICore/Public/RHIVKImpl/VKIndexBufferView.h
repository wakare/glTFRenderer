#pragma once
#include "VolkUtils.h"
#include "RHIInterface/IRHIIndexBufferView.h"

class RHICORE_API VKIndexBufferView : public IRHIIndexBufferView
{
public:
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) override;

    VkBuffer m_buffer;
};

#pragma once
#include <vulkan/vulkan_core.h>
#include "glTFRHI/RHIInterface/IRHIIndexBufferView.h"

class VKIndexBufferView : public IRHIIndexBufferView
{
public:
    virtual bool InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc) override;

    VkBuffer m_buffer;
};

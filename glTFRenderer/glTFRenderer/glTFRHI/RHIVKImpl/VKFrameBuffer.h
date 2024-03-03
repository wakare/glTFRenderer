#pragma once
#include <vulkan/vulkan_core.h>

#include "glTFRHI/RHIInterface/IRHIFrameBuffer.h"

class VKFrameBuffer : public IRHIFrameBuffer
{
public:
    VKFrameBuffer() = default;
    virtual ~VKFrameBuffer() override;
    DECLARE_NON_COPYABLE(VKFrameBuffer)
    
    virtual bool InitFrameBuffer(IRHIDevice& device, IRHISwapChain& swap_chain, const RHIFrameBufferInfo& info) override;

    const VkFramebuffer& GetFrameBuffer() const;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFramebuffer m_frame_buffer {VK_NULL_HANDLE};
};

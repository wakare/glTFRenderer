#pragma once
#include "VolkUtils.h"
#include "glTFRHI/RHIInterface/IRHIFrameBuffer.h"

class VKFrameBuffer : public IRHIFrameBuffer
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VKFrameBuffer)
    
    virtual bool InitFrameBuffer(IRHIDevice& device, IRHISwapChain& swap_chain, const RHIFrameBufferInfo& info) override;

    const VkFramebuffer& GetFrameBuffer() const;
    
    virtual bool Release(glTFRenderResourceManager&) override;
    
protected:
    VkDevice m_device {VK_NULL_HANDLE};
    VkFramebuffer m_frame_buffer {VK_NULL_HANDLE};
};

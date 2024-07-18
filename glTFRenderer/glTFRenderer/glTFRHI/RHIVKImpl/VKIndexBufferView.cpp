#include "VKIndexBufferView.h"

bool VKIndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc)
{
    
    m_desc = desc;
    
    return true;
}

RHIGPUDescriptorHandle VKIndexBufferView::GetGPUHandle() const
{
    return 0;
}

size_t VKIndexBufferView::GetSize() const
{
    return 0;
}

unsigned VKIndexBufferView::GetFormat() const
{
    return 0;
}

#include "VKIndexBufferView.h"
#include "VKBuffer.h"

bool VKIndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc)
{
    m_desc = desc;
    m_buffer = dynamic_cast<VKBuffer&>(buffer).GetRawBuffer();
    return true;
}

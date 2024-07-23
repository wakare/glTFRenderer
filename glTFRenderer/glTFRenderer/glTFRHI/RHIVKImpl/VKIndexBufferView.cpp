#include "VKIndexBufferView.h"

bool VKIndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, const RHIIndexBufferViewDesc& desc)
{
    
    m_desc = desc;
    
    return true;
}
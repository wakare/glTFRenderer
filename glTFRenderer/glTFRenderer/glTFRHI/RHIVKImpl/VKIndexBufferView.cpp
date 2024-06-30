#include "VKIndexBufferView.h"

bool VKIndexBufferView::InitIndexBufferView(IRHIBuffer& buffer, size_t offset, RHIDataFormat indexFormat,
    size_t indexBufferSize)
{
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

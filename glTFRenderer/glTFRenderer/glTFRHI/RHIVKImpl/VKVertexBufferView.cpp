#include "VKVertexBufferView.h"

void VKVertexBufferView::InitVertexBufferView(IRHIBuffer& buffer, size_t offset, size_t vertexStride,
    size_t vertexBufferSize)
{
}

RHIGPUDescriptorHandle VKVertexBufferView::GetGPUHandle() const
{
    return 0;
}

size_t VKVertexBufferView::GetSize() const
{
    return 0;
}

size_t VKVertexBufferView::GetStride() const
{
    return 0;
}

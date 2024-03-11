#include "VKGPUBuffer.h"

bool VKGPUBuffer::InitGPUBuffer(IRHIDevice& device, const RHIBufferDesc& desc)
{
    return true;
}

bool VKGPUBuffer::UploadBufferFromCPU(const void* data, size_t dataOffset, size_t size)
{
    return true;
}

unsigned long long VKGPUBuffer::GetGPUBufferHandle()
{
    return 0;
}

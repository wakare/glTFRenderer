#include "IRHIIndexBuffer.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

IRHIIndexBuffer::IRHIIndexBuffer()
    : m_buffer(nullptr)
    , m_upload_buffer(nullptr)
{
    m_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_upload_buffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
}

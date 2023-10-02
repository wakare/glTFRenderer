#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIGPUBuffer.h"
#include "IRHIIndexBuffer.h"
#include "IRHIResource.h"
#include "IRHIVertexBuffer.h"

class IRHIRayTracingAS : public IRHIResource
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIVertexBuffer& vertex_buffer, IRHIIndexBuffer& index_buffer) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const = 0;
};

#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIGPUBuffer.h"
#include "IRHIResource.h"

class IRHIRayTracingAS : public IRHIResource
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIGPUBuffer& vertex_buffer, IRHIGPUBuffer& index_buffer) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const = 0;
};

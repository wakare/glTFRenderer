#pragma once
#include "glTFRHI/RHIInterface/IRHIRayTracingAS.h"

class VKRayTracingAS : public IRHIRayTracingAS
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager) override;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const override;
};

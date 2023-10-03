#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"
#include "glTFRenderPass/glTFRenderPassMeshResource.h"

class IRHIRayTracingAS : public IRHIResource
{
public:
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const std::map<glTFUniqueID, glTFRenderPassMeshResource>& meshes) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const = 0;
};

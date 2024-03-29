﻿#pragma once
#include "IRHICommandList.h"
#include "IRHIDevice.h"
#include "IRHIResource.h"

class glTFRenderMeshManager;

class IRHIRayTracingAS : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRayTracingAS)
    
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, const glTFRenderMeshManager& mesh_manager) = 0;
    virtual GPU_BUFFER_HANDLE_TYPE GetTLASHandle() const = 0;
};

#pragma once
#include "IRHICommandList.h"
#include "IRHIGPUBuffer.h"
#include "IRHIResource.h"

enum class RHIResourceStateType
{
    COPY_DEST,
    VERTEX_AND_CONSTANT_BUFFER,
};

class IRHIBarrier : public IRHIResource
{
public:
    virtual bool AddBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
};

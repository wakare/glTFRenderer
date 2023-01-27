#pragma once
#include "../RHIInterface/IRHIBarrier.h"

class DX12Barrier : public IRHIBarrier
{
public:
    virtual bool AddBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
};

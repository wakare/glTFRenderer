#include "DX12Barrier.h"

#include "d3dx12.h"
#include "DX12CommandList.h"
#include "DX12GPUBuffer.h"
#include "DX12Utils.h"

bool DX12Barrier::AddBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer,
                                          RHIResourceStateType beforeState, RHIResourceStateType afterState)
{
    auto* dxCommandList = dynamic_cast<DX12CommandList&>(commandList).GetCommandList();
    auto* dxBuffer = dynamic_cast<DX12GPUBuffer&>(buffer).GetBuffer();
    
    CD3DX12_RESOURCE_BARRIER TransitionToVertexBufferState = CD3DX12_RESOURCE_BARRIER::Transition(dxBuffer,
        DX12ConverterUtils::ConvertToResourceState(beforeState), DX12ConverterUtils::ConvertToResourceState(afterState)); 
    dxCommandList->ResourceBarrier(1, &TransitionToVertexBufferState);

    return true;
}

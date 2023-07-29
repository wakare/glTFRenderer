#pragma once

#include <memory>
#include "RHIInterface/RHICommon.h"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICullMode.h"
#include "RHIInterface/IRHIDescriptorHeap.h"
#include "RHIInterface/IRHIIndexBufferView.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIResource.h"
#include "RHIInterface/IRHIVertexBufferView.h"

class IRHIGPUBuffer;
class IRHICommandList;

// Singleton for provide combined basic rhi operations
class RHIUtils : public IRHIResource
{
    friend class RHIResourceFactory;
public:
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator) = 0;
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject& initPSO) = 0;
    virtual bool CloseCommandList(IRHICommandList& commandList) = 0;
    virtual bool ExecuteCommandList(IRHICommandList& commandList, IRHICommandQueue& commandQueue) = 0;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& commandAllocator) = 0;

    virtual bool SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature) = 0;
    virtual bool SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewportDesc) = 0;
    virtual bool SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissorRect) = 0;
    
    virtual bool SetVertexBufferView(IRHICommandList& commandList, IRHIVertexBufferView& view) = 0;
    virtual bool SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view) = 0;
    virtual bool SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type) = 0;
    
    virtual bool SetDescriptorHeapArray(IRHICommandList& commandList, IRHIDescriptorHeap* descriptorArray, size_t descriptorCount) = 0;
    virtual bool SetConstantBufferViewGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle) = 0;
    virtual bool SetShaderResourceViewGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle) = 0;
    virtual bool SetDescriptorTableGPUHandleToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle) = 0;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t size) = 0;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) = 0;

    virtual bool AddBufferBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, IRHIRenderTarget& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;

    virtual bool DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) = 0;
    virtual bool Present(IRHISwapChain& swapchain) = 0;
    
    static RHIUtils& Instance();
    
protected:
    RHIUtils() = default;

    static std::shared_ptr<RHIUtils> g_instance;
};

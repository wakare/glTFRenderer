#pragma once

#include <memory>
#include "RHIInterface/RHICommon.h"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandSignature.h"
#include "RHIInterface/IRHIDescriptorHeap.h"
#include "RHIInterface/IRHIIndexBufferView.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIResource.h"
#include "RHIInterface/IRHIShaderTable.h"
#include "RHIInterface/IRHIVertexBufferView.h"

class IRHIFrameBuffer;
class IRHIBuffer;
class IRHICommandList;

struct RHIExecuteCommandListWaitInfo
{
    const IRHISemaphore* m_wait_semaphore;
    RHIPipelineStage wait_stage;
};

struct RHIExecuteCommandListContext
{
    std::vector<RHIExecuteCommandListWaitInfo> wait_infos;
    std::vector<const IRHISemaphore*> sign_semaphores;
};

struct RHIBeginRenderPassInfo
{
    const IRHIRenderPass* render_pass;
    const IRHIFrameBuffer* frame_buffer;
    unsigned width;
    unsigned height;
};

// Singleton for provide combined basic rhi operations
class RHIUtils : public IRHIResource
{
    friend class RHIResourceFactory;
    
public:
    RHIUtils() = default;
    
    virtual bool InitGUIContext(IRHIDevice& device, IRHIDescriptorHeap& descriptor_heap, unsigned back_buffer_count) = 0;
    virtual bool NewGUIFrame() = 0;
    virtual bool RenderGUIFrame(IRHICommandList& commandList) = 0;
    virtual bool ExitGUI() = 0;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) = 0;
    virtual bool EndRenderPass(IRHICommandList& command_list) = 0;
    
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject* initPSO = nullptr) = 0;
    virtual bool CloseCommandList(IRHICommandList& commandList) = 0;
    virtual bool ExecuteCommandList(IRHICommandList& commandList, IRHICommandQueue& commandQueue, const RHIExecuteCommandListContext& context) = 0;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& commandAllocator) = 0;
    virtual bool WaitCommandListFinish(IRHICommandList& command_queue) = 0;
    
    virtual bool SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature, bool isGraphicsPipeline) = 0;
    virtual bool SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewportDesc) = 0;
    virtual bool SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissorRect) = 0;
    
    virtual bool SetVertexBufferView(IRHICommandList& commandList, unsigned slot, IRHIVertexBufferView& view) = 0;
    virtual bool SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view) = 0;
    virtual bool SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type) = 0;
    
    virtual bool SetDescriptorHeapArray(IRHICommandList& commandList, IRHIDescriptorHeap* descriptorArray, size_t descriptorCount) = 0;
    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, unsigned* data, unsigned count, bool isGraphicsPipeline) = 0;
    virtual bool SetCBVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetSRVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, const IRHIDescriptorTable& table_handle, bool isGraphicsPipeline) = 0;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t size) = 0;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) = 0;

    virtual bool AddBufferBarrierToCommandList(IRHICommandList& commandList, const IRHIBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& commandList, const IRHITexture& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, const IRHIRenderTarget& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;

    virtual bool DrawInstanced(IRHICommandList& commandList, unsigned vertexCountPerInstance, unsigned instanceCount, unsigned startVertexLocation, unsigned startInstanceLocation) = 0;
    virtual bool DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) = 0;
    
    virtual bool Dispatch(IRHICommandList& commandList, unsigned X, unsigned Y, unsigned Z) = 0;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) = 0;
    
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset) = 0;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset) = 0;
    
    virtual bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list) = 0;

    virtual bool DiscardResource(IRHICommandList& commandList, IRHIRenderTarget& render_target) = 0;
    virtual bool CopyTexture(IRHICommandList& commandList, IRHITexture& dst, IRHITexture& src) = 0;
    virtual bool CopyBuffer(IRHICommandList& commandList, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) = 0;

    virtual bool SupportRayTracing(IRHIDevice& device) = 0;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size) = 0;
    
    static RHIUtils& Instance();

protected:
    static std::shared_ptr<RHIUtils> g_instance;
};

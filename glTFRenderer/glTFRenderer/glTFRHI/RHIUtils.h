#pragma once

#include <memory>
#include "RHIInterface/RHICommon.h"
#include "RHIInterface/IRHICommandAllocator.h"
#include "RHIInterface/IRHICommandSignature.h"
#include "RHIInterface/IRHIDescriptorManager.h"
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
    
    virtual bool InitGUIContext(IRHIDevice& device, IRHIDescriptorManager& descriptor_heap, unsigned back_buffer_count) = 0;
    virtual bool NewGUIFrame() = 0;
    virtual bool RenderGUIFrame(IRHICommandList& command_list) = 0;
    virtual bool ExitGUI() = 0;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) = 0;
    virtual bool EndRenderPass(IRHICommandList& command_list) = 0;
    
    virtual bool ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator, IRHIPipelineStateObject* initPSO = nullptr) = 0;
    virtual bool CloseCommandList(IRHICommandList& command_list) = 0;
    virtual bool ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context) = 0;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& command_allocator) = 0;
    virtual bool WaitCommandListFinish(IRHICommandList& command_queue) = 0;
    
    virtual bool SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature, bool isGraphicsPipeline) = 0;
    virtual bool SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewportDesc) = 0;
    virtual bool SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissorRect) = 0;
    
    virtual bool SetVertexBufferView(IRHICommandList& command_list, unsigned slot, IRHIVertexBufferView& view) = 0;
    virtual bool SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view) = 0;
    virtual bool SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type) = 0;
    
    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, unsigned* data, unsigned count, bool isGraphicsPipeline) = 0;
    virtual bool SetCBVToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetSRVToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline) = 0;
    virtual bool SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, const IRHIDescriptorTable& table_handle, bool isGraphicsPipeline) = 0;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t size) = 0;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) = 0;

    virtual bool AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& command_list, const IRHITexture& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;

    virtual bool DrawInstanced(IRHICommandList& command_list, unsigned vertexCountPerInstance, unsigned instanceCount, unsigned startVertexLocation, unsigned startInstanceLocation) = 0;
    virtual bool DrawIndexInstanced(IRHICommandList& command_list, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) = 0;
    
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) = 0;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) = 0;
    
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset) = 0;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset) = 0;
    
    virtual bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list) = 0;

    virtual bool DiscardResource(IRHICommandList& command_list, IRHIRenderTarget& render_target) = 0;
    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src) = 0;
    virtual bool CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) = 0;

    virtual bool SupportRayTracing(IRHIDevice& device) = 0;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size) = 0;

    static RHIUtils& Instance();

protected:
    static std::shared_ptr<RHIUtils> g_instance;
};

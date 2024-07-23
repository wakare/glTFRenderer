#pragma once
#include "glTFRHI/RHIUtils.h"

#define SAFE_RELEASE(x)
#define VK_CHECK(x) {\
VkResult result = (x);\
assert(result == VK_SUCCESS);\
}

class VulkanUtils : public RHIUtils
{
    friend class RHIResourceFactory;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VulkanUtils)

    virtual bool InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_manager, unsigned back_buffer_count) override;
    virtual bool NewGUIFrame() override;
    virtual bool RenderGUIFrame(IRHICommandList& commandList) override;
    virtual bool ExitGUI() override;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) override;
    virtual bool EndRenderPass(IRHICommandList& command_list) override;
    
    virtual bool BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info) override;
    virtual bool EndRendering(IRHICommandList& command_list) override;
    
    virtual bool ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator, IRHIPipelineStateObject* init_pso) override;
    virtual bool CloseCommandList(IRHICommandList& commandList) override;
    virtual bool ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context) override;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& commandAllocator) override;
    virtual bool WaitCommandListFinish(IRHICommandList& command_list) override;
    
    virtual bool SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature,IRHIPipelineStateObject& pipeline_state_object, RHIPipelineType pipeline_type) override;
    virtual bool SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc) override;
    virtual bool SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect) override;

    virtual bool SetVertexBufferView(IRHICommandList& command_list, unsigned slot, IRHIVertexBufferView& view) override;
    virtual bool SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view) override;
    virtual bool SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type) override;

    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, unsigned* data, unsigned count, RHIPipelineType pipeline) override;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t size) override;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIBuffer& uploadBuffer, IRHIBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) override;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& commandList, const IRHIBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& commandList, const IRHITexture& texture, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    
    virtual bool DrawInstanced(IRHICommandList& commandList, unsigned vertexCountPerInstance, unsigned instanceCount, unsigned startVertexLocation, unsigned startInstanceLocation) override;
    virtual bool DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) override;
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) override;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) override;

    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset) override;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset) override;
    
    virtual bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list) override;

    virtual bool CopyTexture(IRHICommandList& commandList, IRHITexture& dst, IRHITexture& src) override;
    virtual bool CopyBuffer(IRHICommandList& commandList, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) override;
    
    virtual bool SupportRayTracing(IRHIDevice& device) override;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size ) override;
};

#pragma once
#include "glTFRHI/RHIUtils.h"

#define VK_CHECK(x) {\
VkResult result = (x);\
assert(result == VK_SUCCESS);\
}

class VulkanUtils : public RHIUtils
{
    friend class RHIResourceFactory;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(VulkanUtils)

    virtual bool InitGraphicsAPI() override;
    
    virtual bool InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_manager, unsigned back_buffer_count) override;
    virtual bool NewGUIFrame() override;
    virtual bool RenderGUIFrame(IRHICommandList& command_list) override;
    virtual bool ExitGUI() override;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) override;
    virtual bool EndRenderPass(IRHICommandList& command_list) override;
    
    virtual bool BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info) override;
    virtual bool EndRendering(IRHICommandList& command_list) override;
    
    virtual bool ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator, IRHIPipelineStateObject* init_pso) override;
    virtual bool CloseCommandList(IRHICommandList& command_list) override;
    virtual bool ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context) override;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& command_allocator) override;
    virtual bool WaitCommandListFinish(IRHICommandList& command_list) override;
    
    virtual bool SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature, IRHIPipelineStateObject& pipeline_state_object, RHIPipelineType pipeline_type) override;
    virtual bool SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc) override;
    virtual bool SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect) override;

    virtual bool SetVertexBufferView(IRHICommandList& command_list, unsigned binding_slot_index, IRHIVertexBufferView& view) override;
    virtual bool SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view) override;
    virtual bool SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type) override;

    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, unsigned* data, unsigned count, RHIPipelineType pipeline) override;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer, RHIResourceStateType before_state, RHIResourceStateType after_state) override;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& command_list, IRHITexture& texture, RHIResourceStateType before_state, RHIResourceStateType after_state) override;
    
    virtual bool DrawInstanced(IRHICommandList& command_list, unsigned vertex_count_per_instance, unsigned instance_count, unsigned start_vertex_location, unsigned start_instance_location) override;
    virtual bool DrawIndexInstanced(IRHICommandList& command_list, unsigned index_count_per_instance, unsigned instance_count, unsigned start_index_location, unsigned base_vertex_location, unsigned start_instance_location) override;
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) override;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) override;

    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, unsigned command_stride) override;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset, unsigned
                                 command_stride) override;
    
    virtual bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list) override;

    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src) override;
    virtual bool CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) override;
    
    virtual bool SupportRayTracing(IRHIDevice& device) override;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size ) override;

    virtual void ReportLiveObjects() override;
};

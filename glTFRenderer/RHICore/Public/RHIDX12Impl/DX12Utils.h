#pragma once
#include "RHIUtils.h"

class IRHIDescriptorTable;
class DX12DescriptorHeap;
class IRHIDescriptorAllocation;

enum class RHIBufferType;
enum class RHIDataFormat;

#define SAFE_RELEASE(x) if ((x)) {/*(x)->Release();*/ (x).Reset(); LOG_FORMAT_FLUSH("[Exit] Resource %s Release %s\n", GetName().c_str(), #x)}

class DX12Utils : public RHIUtils
{
    friend class RHIResourceFactory;
    
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Utils)
    
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
    virtual bool WaitCommandQueueIdle(IRHICommandQueue& command_queue) override;
    virtual bool WaitDeviceIdle(IRHIDevice& device) override;

    virtual bool SetPipelineState(IRHICommandList& command_list, IRHIPipelineStateObject& pipeline_state_object) override;
    virtual bool SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& rootSignature, IRHIPipelineStateObject& pipeline_state_object,RHIPipelineType pipeline_type) override;
    virtual bool SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc) override;
    virtual bool SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect) override;

    virtual bool SetVertexBufferView(IRHICommandList& command_list, unsigned slot, IRHIVertexBufferView& view) override;
    virtual bool SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view) override;
    virtual bool SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type) override;

    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, unsigned* data, unsigned count, RHIPipelineType pipeline_type) override;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& command_list, IRHITexture& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool AddUAVBarrier(IRHICommandList& command_list, IRHITexture& texture) override;
    
    virtual bool DrawInstanced(IRHICommandList& command_list, unsigned vertex_count_per_instance, unsigned instance_count, unsigned start_vertex_location, unsigned start_instance_location) override;
    virtual bool DrawIndexInstanced(IRHICommandList& command_list, unsigned index_count_per_instance, unsigned instance_count, unsigned start_index_location, unsigned base_vertex_location, unsigned start_instance_location) override;
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) override;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) override;

    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, unsigned command_stride) override;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset, unsigned
                                 command_stride) override;
    
    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src, const RHICopyTextureInfo& copy_info) override;
    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHIBuffer& src, const RHICopyTextureInfo& copy_info) override;
    virtual bool CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) override;
    
    virtual bool ClearUAVTexture(IRHICommandList& command_list, const IRHITextureDescriptorAllocation& texture_descriptor) override;
    
    virtual bool SupportRayTracing(IRHIDevice& device) override;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size ) override;

    virtual void ReportLiveObjects() override;
    virtual bool ProcessShaderMetaData(IRHIShader& shader) override;
    
    // DX12 private implementation
    static DX12Utils& DX12Instance();

    bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& upload_buffer, IRHIBuffer& default_buffer, void* data, size_t size);
    bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& command_list, IRHIBuffer& upload_buffer, IRHIBuffer& default_buffer, void* data, size_t row_pitch, size_t slice_pitch);
    
    bool SetDescriptorHeapArray(IRHICommandList& command_list, DX12DescriptorHeap* descriptor_heap_array_data, size_t descriptor_heap_array_count);
    bool SetCBVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline);
    bool SetSRVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline);
    bool SetUAVToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline);
    bool SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, const IRHIDescriptorAllocation& handle, bool isGraphicsPipeline);
    bool SetDTToRootParameterSlot(IRHICommandList& command_list, unsigned slot_index, const IRHIDescriptorTable& table_handle, bool isGraphicsPipeline);
};

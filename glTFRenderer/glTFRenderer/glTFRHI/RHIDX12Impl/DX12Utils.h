#pragma once
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHICommandSignature.h"
//#include "glTFUtils/glTFLog.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"

enum class RHIBufferType;
enum class RHIDataFormat;

#define SAFE_RELEASE(x)
//#define SAFE_RELEASE(x) if ((x)) {(x)->Release(); (x) = nullptr; LOG_FORMAT_FLUSH("[Exit] Resource %s Release %s\n", GetName().c_str(), #x)}

class DX12Utils : public RHIUtils
{
    friend class RHIResourceFactory;
    
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DX12Utils)
    
    virtual bool InitGUIContext(IRHIDevice& device, IRHIDescriptorHeap& descriptor_heap, unsigned back_buffer_count) override;
    virtual bool NewGUIFrame() override;
    virtual bool RenderGUIFrame(IRHICommandList& commandList) override;
    virtual bool ExitGUI() override;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) override;
    virtual bool EndRenderPass(IRHICommandList& command_list) override;
    
    virtual bool ResetCommandList(IRHICommandList& commandList, IRHICommandAllocator& commandAllocator, IRHIPipelineStateObject* initPSO) override;
    virtual bool CloseCommandList(IRHICommandList& commandList) override;
    virtual bool ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context) override;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& commandAllocator) override;
    virtual bool WaitCommandListFinish(IRHICommandList& command_queue) override;
    
    virtual bool SetRootSignature(IRHICommandList& commandList, IRHIRootSignature& rootSignature, bool isGraphicsPipeline) override;
    virtual bool SetViewport(IRHICommandList& commandList, const RHIViewportDesc& viewport_desc) override;
    virtual bool SetScissorRect(IRHICommandList& commandList, const RHIScissorRectDesc& scissor_rect) override;

    virtual bool SetVertexBufferView(IRHICommandList& commandList, unsigned slot, IRHIVertexBufferView& view) override;
    virtual bool SetIndexBufferView(IRHICommandList& commandList, IRHIIndexBufferView& view) override;
    virtual bool SetPrimitiveTopology(IRHICommandList& commandList, RHIPrimitiveTopologyType type) override;

    virtual bool SetDescriptorHeapArray(IRHICommandList& commandList, IRHIDescriptorHeap* descriptor_heap_array_data, size_t descriptor_heap_array_count) override;
    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, unsigned* data, unsigned count, bool isGraphicsPipeline) override;
    virtual bool SetCBVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle, bool isGraphicsPipeline) override;
    virtual bool SetSRVToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle, bool isGraphicsPipeline) override;
    virtual bool SetDTToRootParameterSlot(IRHICommandList& commandList, unsigned slotIndex, RHIGPUDescriptorHandle handle, bool isGraphicsPipeline) override;
    
    virtual bool UploadBufferDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t size) override;
    virtual bool UploadTextureDataToDefaultGPUBuffer(IRHICommandList& commandList, IRHIGPUBuffer& uploadBuffer, IRHIGPUBuffer& defaultBuffer, void* data, size_t rowPitch, size_t slicePitch) override;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& commandList, IRHIGPUBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) override;
    virtual bool AddRenderTargetBarrierToCommandList(IRHICommandList& commandList, IRHIRenderTarget& buffer, RHIResourceStateType before_state, RHIResourceStateType after_state) override;

    virtual bool DrawInstanced(IRHICommandList& commandList, unsigned vertexCountPerInstance, unsigned instanceCount, unsigned startVertexLocation, unsigned startInstanceLocation) override;
    virtual bool DrawIndexInstanced(IRHICommandList& commandList, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) override;
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) override;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) override;

    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIGPUBuffer& arguments_buffer, unsigned arguments_buffer_offset) override;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIGPUBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIGPUBuffer& count_buffer, unsigned count_buffer_offset) override;
    
    virtual bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list) override;

    virtual bool DiscardResource(IRHICommandList& command_list, IRHIRenderTarget& render_target) override;

    virtual bool CopyTexture(IRHICommandList& commandList, IRHIRenderTarget& dst, IRHIRenderTarget& src) override;
    virtual bool CopyBuffer(IRHICommandList& commandList, IRHIGPUBuffer& dst, size_t dst_offset, IRHIGPUBuffer& src, size_t src_offset, size_t size) override;
    
    virtual bool SupportRayTracing(IRHIDevice& device) override;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size ) override;
};

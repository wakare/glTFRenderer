#pragma once

#include <memory>

#include "RHIInterface/IRHIResource.h"
#include "RHIInterface/IRHITexture.h"
#include "RHICommon.h"

class IRHIShader;
class IRHIRootSignatureHelper;
class IRHISwapChain;
class IRHIShaderTable;
class IRHICommandSignature;
class IRHITexture;
class IRHIVertexBufferView;
class IRHIIndexBufferView;
class IRHIRootSignature;
class IRHIPipelineStateObject;
class IRHICommandAllocator;
class IRHIDescriptorManager;
class IRHICommandQueue;
class IRHIDevice;
class IRHIFrameBuffer;
class IRHIBuffer;
class IRHICommandList;

// Singleton for provide combined basic rhi operations
class RHICORE_API RHIUtils
{
    friend class RHIResourceFactory;
    friend class RHIUtilInstanceManager;
    
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(RHIUtils)

    virtual bool InitGraphicsAPI() = 0;
    
    virtual bool InitGUIContext(IRHIDevice& device, IRHICommandQueue& graphics_queue, IRHIDescriptorManager& descriptor_heap, unsigned back_buffer_count) = 0;
    virtual bool NewGUIFrame() = 0;
    virtual bool RenderGUIFrame(IRHICommandList& command_list) = 0;
    virtual bool ExitGUI() = 0;

    virtual bool BeginRenderPass(IRHICommandList& command_list, const RHIBeginRenderPassInfo& begin_render_pass_info) = 0;
    virtual bool EndRenderPass(IRHICommandList& command_list) = 0;

    virtual bool BeginRendering(IRHICommandList& command_list, const RHIBeginRenderingInfo& begin_rendering_info) = 0;
    virtual bool EndRendering(IRHICommandList& command_list) = 0;
    
    virtual bool ResetCommandList(IRHICommandList& command_list, IRHICommandAllocator& command_allocator, IRHIPipelineStateObject* initPSO = nullptr) = 0;
    virtual bool CloseCommandList(IRHICommandList& command_list) = 0;
    virtual bool ExecuteCommandList(IRHICommandList& command_list, IRHICommandQueue& command_queue, const RHIExecuteCommandListContext& context) = 0;
    virtual bool ResetCommandAllocator(IRHICommandAllocator& command_allocator) = 0;
    virtual bool WaitCommandListFinish(IRHICommandList& command_list) = 0;
    virtual bool WaitCommandQueueIdle(IRHICommandQueue& command_queue) = 0;
    virtual bool WaitDeviceIdle(IRHIDevice& device) = 0;
    
    virtual bool SetRootSignature(IRHICommandList& command_list, IRHIRootSignature& root_signature, IRHIPipelineStateObject& pipeline_state_object, RHIPipelineType pipeline_type) = 0;
    virtual bool SetViewport(IRHICommandList& command_list, const RHIViewportDesc& viewport_desc) = 0;
    virtual bool SetScissorRect(IRHICommandList& command_list, const RHIScissorRectDesc& scissor_rect) = 0;
    
    virtual bool SetVertexBufferView(IRHICommandList& command_list, unsigned slot, IRHIVertexBufferView& view) = 0;
    virtual bool SetIndexBufferView(IRHICommandList& command_list, IRHIIndexBufferView& view) = 0;
    virtual bool SetPrimitiveTopology(IRHICommandList& command_list, RHIPrimitiveTopologyType type) = 0;
    
    virtual bool SetConstant32BitToRootParameterSlot(IRHICommandList& command_list, unsigned slotIndex, unsigned* data, unsigned count, RHIPipelineType pipeline_type) = 0;
    
    virtual bool AddBufferBarrierToCommandList(IRHICommandList& command_list, const IRHIBuffer& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddTextureBarrierToCommandList(IRHICommandList& command_list, IRHITexture& buffer, RHIResourceStateType beforeState, RHIResourceStateType afterState) = 0;
    virtual bool AddUAVBarrier(IRHICommandList& command_list, IRHITexture& texture) = 0;

    virtual bool DrawInstanced(IRHICommandList& command_list, unsigned vertexCountPerInstance, unsigned instanceCount, unsigned startVertexLocation, unsigned startInstanceLocation) = 0;
    virtual bool DrawIndexInstanced(IRHICommandList& command_list, unsigned indexCountPerInstance, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation, unsigned startInstanceLocation) = 0;
    
    virtual bool Dispatch(IRHICommandList& command_list, unsigned X, unsigned Y, unsigned Z) = 0;
    virtual bool TraceRay(IRHICommandList& command_list, IRHIShaderTable& shader_table, unsigned X, unsigned Y, unsigned Z) = 0;
    
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, unsigned command_stride) = 0;
    virtual bool ExecuteIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, unsigned max_count, IRHIBuffer& arguments_buffer, unsigned arguments_buffer_offset, IRHIBuffer& count_buffer, unsigned count_buffer_offset, unsigned
                                 command_stride) = 0;
    
    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHITexture& src, const RHICopyTextureInfo& copy_info) = 0;
    virtual bool CopyTexture(IRHICommandList& command_list, IRHITexture& dst, IRHIBuffer& src, const RHICopyTextureInfo& copy_info) = 0;
    virtual bool CopyBuffer(IRHICommandList& command_list, IRHIBuffer& dst, size_t dst_offset, IRHIBuffer& src, size_t src_offset, size_t size) = 0;

    virtual bool ClearUAVTexture(IRHICommandList& command_list, const IRHITextureDescriptorAllocation& texture_descriptor) = 0;
    
    virtual bool SupportRayTracing(IRHIDevice& device) = 0;
    virtual unsigned GetAlignmentSizeForUAVCount(unsigned size) = 0;

    virtual bool ProcessShaderMetaData(IRHIShader& shader) = 0;
    
    virtual void ReportLiveObjects() = 0;

    bool UploadTextureData(IRHICommandList& command_list, IRHIMemoryManager& memory_manager, IRHIDevice& device, IRHITexture& dst, const RHITextureMipUploadInfo& upload_info) ;
    bool Present(IRHISwapChain& swap_chain, IRHICommandQueue& command_queue, IRHICommandList& command_list);
};

class RHICORE_API RHIUtilInstanceManager
{
public:
    static RHIUtils& Instance();
    static void ResetInstance();
};

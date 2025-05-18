#pragma once
#include <memory>

#include "IRHICommandList.h"
#include "IRHIRenderTarget.h"
#include "IRHIRootSignatureHelper.h"

class IRHIDescriptorUpdater;
class glTFRenderResourceManager;

namespace glTFRenderResourceUtils
{
    struct GBufferSignatureAllocationWithinPass
    {
        bool InitGBufferAllocation(glTFUniqueID pass_id, IRHIRootSignatureHelper& root_signature_helper, bool asSRV);
        bool UpdateShaderMacros(glTFUniqueID pass_id, RHIShaderPreDefineMacros& macros, bool asSRV) const;
        
        RootSignatureAllocation m_albedo_allocation;
        RootSignatureAllocation m_normal_allocation;
        RootSignatureAllocation m_depth_allocation;
    };

    struct GBufferSignatureAllocations
    {
        bool InitGBufferAllocation(glTFUniqueID pass_id, IRHIRootSignatureHelper& root_signature_helper, bool asSRV);
        bool UpdateShaderMacros(glTFUniqueID pass_id, RHIShaderPreDefineMacros& macros, bool asSRV) const;

        GBufferSignatureAllocationWithinPass& GetAllocationWithPassId(glTFUniqueID pass_id);
        
        std::map<glTFUniqueID, GBufferSignatureAllocationWithinPass> m_allocations;
    };
    
    struct GBufferResourceWithinPass
    {
        std::shared_ptr<IRHITextureDescriptorAllocation> m_albedo_handle;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_normal_handle;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_depth_handle;
    };
    
    struct GBufferOutput
    {
        std::shared_ptr<IRHITextureDescriptorAllocation> m_albedo_output;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_normal_output;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_depth_output;

        bool InitGBufferOutput(glTFRenderResourceManager& resource_manager, unsigned back_buffer_index);
        bool InitGBufferUAVs(glTFUniqueID pass_id, glTFRenderResourceManager& resource_manager);
        bool InitGBufferSRVs(glTFUniqueID pass_id, glTFRenderResourceManager& resource_manager);
        
        bool Transition(glTFUniqueID pass_id, IRHICommandList& command_list, RHIResourceStateType after) const;
        bool Bind(glTFUniqueID pass_id, RHIPipelineType pipeline_type, IRHICommandList& command_list, IRHIDescriptorUpdater& updater, const GBufferSignatureAllocationWithinPass& allocation) const;
        
    protected:
        GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id);
        const GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id) const; 
        
        // descriptor within specific pass
        std::map<glTFUniqueID, GBufferResourceWithinPass> m_GBuffer_pass_resource;
    };

    struct RWTextureResourceWithBackBuffer
    {
        RWTextureResourceWithBackBuffer(std::string output_register_name, std::string back_register_name);
    
        bool CreateResource(glTFRenderResourceManager& resource_manager, const RHITextureDesc& desc);
        bool CreateDescriptors(glTFRenderResourceManager& resource_manager);
        bool RegisterSignature(IRHIRootSignatureHelper& root_signature);
        bool AddShaderMacros(RHIShaderPreDefineMacros& macros);
        bool BindDescriptors(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& updater);
        bool PreRendering(IRHICommandList& command_list);
        bool PostRendering(glTFRenderResourceManager& resource_manager);

    protected:
        std::string m_output_register_name;
        std::string m_back_register_name;
    
        std::string GetOutputBufferResourceName() const;
        std::string GetBackBufferResourceName() const;
    
        RHITextureDesc m_texture0_desc;
        RHITextureDesc m_texture1_desc;
    
        std::shared_ptr<IRHITextureDescriptorAllocation> m_writable_buffer;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_back_buffer;

        std::shared_ptr<IRHITextureDescriptorAllocation> m_writable_buffer_handle;
        std::shared_ptr<IRHITextureDescriptorAllocation> m_back_buffer_handle;
    
        RootSignatureAllocation m_writable_buffer_allocation;
        RootSignatureAllocation m_back_buffer_allocation;
    };
}

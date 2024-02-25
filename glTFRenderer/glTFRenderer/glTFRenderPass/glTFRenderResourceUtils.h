#pragma once
#include <memory>

#include "glTFRHI/RHIInterface/IRHICommandList.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"
#include "glTFRHI/RHIInterface/IRHIShader.h"

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
        RHIGPUDescriptorHandle m_albedo_handle;
        RHIGPUDescriptorHandle m_normal_handle;
        RHIGPUDescriptorHandle m_depth_handle;
    };
    
    struct GBufferOutput
    {
        std::shared_ptr<IRHIRenderTarget> m_albedo_output;
        std::shared_ptr<IRHIRenderTarget> m_normal_output;
        std::shared_ptr<IRHIRenderTarget> m_depth_output;

        bool InitGBufferOutput(glTFRenderResourceManager& resource_manager, unsigned back_buffer_index);
        bool InitGBufferUAVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager);
        bool InitGBufferSRVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager);
        
        bool Transition(glTFUniqueID pass_id, IRHICommandList& command_list, RHIResourceStateType after) const;
        bool Bind(glTFUniqueID pass_id, IRHICommandList& command_list, const GBufferSignatureAllocationWithinPass& allocation) const;
        
    protected:
        GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id);
        const GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id) const; 
        
        // descriptor within specific pass
        std::map<glTFUniqueID, GBufferResourceWithinPass> m_GBuffer_pass_resource;
        
        mutable RHIResourceStateType m_resource_state;
    };

    struct RWTextureResourceWithBackBuffer
    {
        RWTextureResourceWithBackBuffer(std::string output_register_name, std::string back_register_name);
    
        bool CreateResource(glTFRenderResourceManager& resource_manager, const RHIRenderTargetDesc& desc);
        bool CreateDescriptors(glTFRenderResourceManager& resource_manager, IRHIDescriptorHeap& main_descriptor);
        bool RegisterSignature(IRHIRootSignatureHelper& root_signature);
        bool AddShaderMacros(RHIShaderPreDefineMacros& macros);
        bool BindRootParameter(glTFRenderResourceManager& resource_manager);
        bool CopyToBackBuffer(glTFRenderResourceManager& resource_manager);

    protected:
        std::string m_output_register_name;
        std::string m_back_register_name;
    
        std::string GetOutputBufferResourceName() const;
        std::string GetBackBufferResourceName() const;
    
        RHIRenderTargetDesc m_texture_desc;
    
        std::shared_ptr<IRHIRenderTarget> m_writable_buffer;
        std::shared_ptr<IRHIRenderTarget> m_back_buffer;

        RHIGPUDescriptorHandle m_writable_buffer_handle;
        RHIGPUDescriptorHandle m_back_buffer_handle;
    
        RootSignatureAllocation m_writable_buffer_allocation;
        RootSignatureAllocation m_back_buffer_allocation;
    };
}

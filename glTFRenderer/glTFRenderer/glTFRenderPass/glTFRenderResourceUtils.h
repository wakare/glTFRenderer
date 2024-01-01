#pragma once
#include <memory>

#include "glTFRHI/RHIInterface/IRHIDescriptorHeap.h"
#include "glTFRHI/RHIInterface/IRHIRenderTarget.h"
#include "glTFRHI/RHIInterface/IRHIRootSignatureHelper.h"
#include "glTFRHI/RHIInterface/IRHIShader.h"

class glTFRenderResourceManager;

namespace glTFRenderResourceUtils
{
    struct GBufferResourceWithinPass
    {
        RHIGPUDescriptorHandle m_albedo_handle;
        RootSignatureAllocation m_albedo_allocation;

        RHIGPUDescriptorHandle m_normal_handle;
        RootSignatureAllocation m_normal_allocation;
        
        RHIGPUDescriptorHandle m_depth_handle;
        RootSignatureAllocation m_depth_allocation;
    };
    
    struct GBufferOutput
    {
        std::shared_ptr<IRHIRenderTarget> m_albedo_output;
        std::shared_ptr<IRHIRenderTarget> m_normal_output;
        std::shared_ptr<IRHIRenderTarget> m_depth_output;

        bool InitGBufferOutput(glTFRenderResourceManager& resource_manager, unsigned back_buffer_index);
        bool InitGBufferUAVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager);
        bool InitGBufferSRVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager);
        bool InitGBufferAllocation(glTFUniqueID pass_id, IRHIRootSignatureHelper& root_signature_helper, bool asSRV);
        
        bool UpdateShaderMacros(glTFUniqueID pass_id, RHIShaderPreDefineMacros& macros, bool asSRV) const;
        bool Transition(glTFUniqueID pass_id, IRHICommandList& command_list, RHIResourceStateType after) const;
        bool Bind(glTFUniqueID pass_id, IRHICommandList& command_list) const;
        
    protected:
        GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id);
        const GBufferResourceWithinPass& GetGBufferPassResource(glTFUniqueID id) const; 
        
        // descriptor within specific pass
        std::map<glTFUniqueID, GBufferResourceWithinPass> m_GBuffer_pass_resource;
        
        mutable RHIResourceStateType m_resource_state;
    };
}

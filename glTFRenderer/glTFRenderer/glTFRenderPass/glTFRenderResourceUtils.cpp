#include "glTFRenderResourceUtils.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIRenderTargetManager.h"

namespace glTFRenderResourceUtils
{
    bool GBufferSignatureAllocationWithinPass::InitGBufferAllocation(glTFUniqueID pass_id,
        IRHIRootSignatureHelper& root_signature_helper, bool asSRV)
    {
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("Albedo_Output", asSRV ? RHIRootParameterDescriptorRangeType::SRV : RHIRootParameterDescriptorRangeType::UAV, 1, false, m_albedo_allocation))
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("Normal_Output", asSRV ? RHIRootParameterDescriptorRangeType::SRV : RHIRootParameterDescriptorRangeType::UAV, 1, false, m_normal_allocation))
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("Depth_Output", asSRV ? RHIRootParameterDescriptorRangeType::SRV : RHIRootParameterDescriptorRangeType::UAV, 1, false, m_depth_allocation))
    
        return true;
    }

    bool GBufferSignatureAllocationWithinPass::UpdateShaderMacros(glTFUniqueID pass_id,
                                                                  RHIShaderPreDefineMacros& macros, bool asSRV) const
    {
        if (asSRV)
        {
            macros.AddSRVRegisterDefine("ALBEDO_REGISTER_INDEX", m_albedo_allocation.register_index, m_albedo_allocation.space);
            macros.AddSRVRegisterDefine("NORMAL_REGISTER_INDEX", m_normal_allocation.register_index, m_normal_allocation.space);
            macros.AddSRVRegisterDefine("DEPTH_REGISTER_INDEX", m_depth_allocation.register_index, m_depth_allocation.space);    
        }
        else
        {
            macros.AddUAVRegisterDefine("ALBEDO_REGISTER_INDEX", m_albedo_allocation.register_index, m_albedo_allocation.space);
            macros.AddUAVRegisterDefine("NORMAL_REGISTER_INDEX", m_normal_allocation.register_index, m_normal_allocation.space);
            macros.AddUAVRegisterDefine("DEPTH_REGISTER_INDEX", m_depth_allocation.register_index, m_depth_allocation.space);
        }
    
        return true;
    }

    GBufferSignatureAllocationWithinPass& GBufferSignatureAllocations::GetAllocationWithPassId(glTFUniqueID pass_id)
    {
        return m_allocations[pass_id];
    }

    bool GBufferSignatureAllocations::InitGBufferAllocation(glTFUniqueID pass_id,
                                                           IRHIRootSignatureHelper& root_signature_helper, bool asSRV)
    {
        return m_allocations[pass_id].InitGBufferAllocation(pass_id, root_signature_helper, asSRV);
    }

    bool GBufferSignatureAllocations::UpdateShaderMacros(glTFUniqueID pass_id, RHIShaderPreDefineMacros& macros,
        bool asSRV) const
    {
        const auto find_it = m_allocations.find(pass_id);
        GLTF_CHECK(find_it!= m_allocations.end());
        return find_it->second.UpdateShaderMacros(pass_id, macros, asSRV);
    }

    bool GBufferOutput::InitGBufferOutput(glTFRenderResourceManager& resource_manager, unsigned back_buffer_index)
    {
        RHIDataFormat albedo_format = RHIDataFormat::R8G8B8A8_UNORM;
        RHITextureDesc albedo_output_desc
        {
            resource_manager.GetSwapChain().GetWidth(),
            resource_manager.GetSwapChain().GetHeight(),
            albedo_format,
            true,
            {
                .clear_format = albedo_format,
                .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
            },
            "ALBEDO_OUTPUT"
        };
        m_albedo_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), RHIRenderTargetType::RTV, albedo_format, albedo_format, albedo_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Albedo_Output", m_albedo_output, back_buffer_index);
        
        RHIDataFormat normal_format = RHIDataFormat::R8G8B8A8_UNORM;
        RHITextureDesc normal_output_desc
        {
            resource_manager.GetSwapChain().GetWidth(),
            resource_manager.GetSwapChain().GetHeight(),
            normal_format,
            true,
            {
                .clear_format = normal_format,
                .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
            },
            "NORMAL_OUTPUT"
        };
        m_normal_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), RHIRenderTargetType::RTV, normal_format, normal_format, normal_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Normal_Output", m_normal_output, back_buffer_index);
        
        RHIDataFormat depth_format = RHIDataFormat::R32_FLOAT;
        RHITextureDesc depth_output_desc
        {
            resource_manager.GetSwapChain().GetWidth(),
            resource_manager.GetSwapChain().GetHeight(),
            depth_format,
            true,
            {
                .clear_format = depth_format,
                .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
            },
            "DEPTH_OUTPUT"
        };
        m_depth_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), RHIRenderTargetType::RTV, depth_format, depth_format, depth_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Depth_Output", m_depth_output, back_buffer_index);
        
        m_resource_state = RHIResourceStateType::STATE_RENDER_TARGET;
        
        return true;
    }

    bool GBufferOutput::InitGBufferUAVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_albedo_output,
                {
                    m_albedo_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                },
                GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_normal_output,
                {
                    m_normal_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                },
                GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_depth_output,
                {
                    m_depth_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV
                },
                GBufferPassResource.m_depth_handle))

        return true;
    }

    bool GBufferOutput::InitGBufferSRVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap,
        glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_albedo_output,
                {
                    m_albedo_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV
                },
                GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_normal_output,
                {
                    m_normal_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV
                },
                GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
                resource_manager.GetDevice(),
                *m_depth_output,
                {
                    m_depth_output->GetRenderTargetFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV
                },
                GBufferPassResource.m_depth_handle))

        return true;
    }

    bool GBufferOutput::Transition(glTFUniqueID pass_id,IRHICommandList& command_list, RHIResourceStateType after) const
    {
        if (m_resource_state == after)
        {
            return true;
        }
        
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_albedo_output, m_resource_state, after);
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_normal_output, m_resource_state, after);
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_depth_output, m_resource_state, after);

        m_resource_state = after;
        
        return true;
    }

    bool GBufferOutput::Bind(glTFUniqueID pass_id,IRHICommandList& command_list, const GBufferSignatureAllocationWithinPass& allocation) const
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, allocation.m_albedo_allocation.parameter_index, GBufferPassResource.m_albedo_handle, false))
        RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, allocation.m_normal_allocation.parameter_index, GBufferPassResource.m_normal_handle, false))
        RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, allocation.m_depth_allocation.parameter_index, GBufferPassResource.m_depth_handle, false))
    
        return true;
    }

    GBufferResourceWithinPass& GBufferOutput::GetGBufferPassResource(glTFUniqueID id)
    {
        return m_GBuffer_pass_resource[id];
    }

    const GBufferResourceWithinPass& GBufferOutput::GetGBufferPassResource(glTFUniqueID id) const
    {
        const auto find_it = m_GBuffer_pass_resource.find(id);
        GLTF_CHECK(find_it != m_GBuffer_pass_resource.end());

        return find_it->second;
    }

    RWTextureResourceWithBackBuffer::RWTextureResourceWithBackBuffer(std::string output_register_name,
                                                                 std::string back_register_name)
    : m_output_register_name(std::move(output_register_name))
    , m_back_register_name(std::move(back_register_name))
    , m_texture_desc()
    , m_writable_buffer_handle(0)
    , m_back_buffer_handle(0)
    {
    }

    bool RWTextureResourceWithBackBuffer::CreateResource(glTFRenderResourceManager& resource_manager, const RHITextureDesc& desc)
    {
        m_texture_desc = desc;
        auto format = m_texture_desc.GetClearValue().clear_format;
    
        m_writable_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), RHIRenderTargetType::RTV, format, format, m_texture_desc);

        m_back_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), RHIRenderTargetType::RTV, format, format, m_texture_desc);

        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_writable_buffer,
                    RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_UNORDERED_ACCESS))

        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resource_manager.GetCommandListForRecord(), *m_back_buffer,
                    RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
        return true;
    }

    bool RWTextureResourceWithBackBuffer::CreateDescriptors(glTFRenderResourceManager& resource_manager,
        IRHIDescriptorHeap& main_descriptor)
    {
        RETURN_IF_FALSE(main_descriptor.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), *m_writable_buffer,
                        {m_writable_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV}, m_writable_buffer_handle))

        RETURN_IF_FALSE(main_descriptor.CreateShaderResourceViewInDescriptorHeap(resource_manager.GetDevice(), *m_back_buffer,
                        {m_back_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_back_buffer_handle))

        return true;
    }

    bool RWTextureResourceWithBackBuffer::RegisterSignature(IRHIRootSignatureHelper& root_signature)
    {
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetOutputBufferResourceName(), RHIRootParameterDescriptorRangeType::UAV, 1, false, m_writable_buffer_allocation))
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetBackBufferResourceName(), RHIRootParameterDescriptorRangeType::SRV, 1, false, m_back_buffer_allocation))
    
        return true;
    }

    bool RWTextureResourceWithBackBuffer::CopyToBackBuffer(glTFRenderResourceManager& resource_manager)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
    
        // Copy accumulation buffer to back buffer
        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_writable_buffer,
            RHIResourceStateType::STATE_UNORDERED_ACCESS, RHIResourceStateType::STATE_COPY_SOURCE))

        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_back_buffer,
            RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE, RHIResourceStateType::STATE_COPY_DEST))

        RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, *m_back_buffer,  *m_writable_buffer))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_writable_buffer,
            RHIResourceStateType::STATE_COPY_SOURCE, RHIResourceStateType::STATE_UNORDERED_ACCESS))
    
        RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(command_list, *m_back_buffer,
            RHIResourceStateType::STATE_COPY_DEST, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE))
    
        return true;
    }

    std::string RWTextureResourceWithBackBuffer::GetOutputBufferResourceName() const
    {
        return m_texture_desc.GetName();
    }

    std::string RWTextureResourceWithBackBuffer::GetBackBufferResourceName() const
    {
        return GetOutputBufferResourceName() + "_BACK";
    }

    bool RWTextureResourceWithBackBuffer::AddShaderMacros(RHIShaderPreDefineMacros& macros)
    {
        macros.AddUAVRegisterDefine(m_output_register_name, m_writable_buffer_allocation.register_index, m_writable_buffer_allocation.space);
        macros.AddSRVRegisterDefine(m_back_register_name, m_back_buffer_allocation.register_index, m_back_buffer_allocation.space);
    
        return true;
    }

    bool RWTextureResourceWithBackBuffer::BindRootParameter(glTFRenderResourceManager& resource_manager)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
    
        RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_writable_buffer_allocation.parameter_index, m_writable_buffer_handle,false))
        RETURN_IF_FALSE(RHIUtils::Instance().SetDTToRootParameterSlot(command_list, m_back_buffer_allocation.parameter_index, m_back_buffer_handle,false))
    
        return true;
    }
}

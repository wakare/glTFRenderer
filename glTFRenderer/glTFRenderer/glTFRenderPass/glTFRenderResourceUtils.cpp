#include "glTFRenderResourceUtils.h"

#include "glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIDescriptorUpdater.h"
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
        m_albedo_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager,
            RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager), {
                .type = RHIRenderTargetType::RTV,
                .format = RHIDataFormat::UNKNOWN
            });
        
        m_normal_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager,
            RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager), {
                .type = RHIRenderTargetType::RTV,
                .format = RHIDataFormat::UNKNOWN
            });
        
        RHIDataFormat depth_format = RHIDataFormat::R32_FLOAT;
        RHITextureDesc depth_output_desc
        {
            "DEPTH_OUTPUT",
            resource_manager.GetSwapChain().GetWidth(),
            resource_manager.GetSwapChain().GetHeight(),
            depth_format,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_ALLOW_RENDER_TARGET),
            {
                .clear_format = depth_format,
                .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
            }
        };
        m_depth_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager,
            depth_output_desc, {
                .type = RHIRenderTargetType::RTV,
                .format = RHIDataFormat::UNKNOWN
            });
        
        return true;
    }

    bool GBufferOutput::InitGBufferUAVs(glTFUniqueID pass_id, glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        *m_albedo_output,
                        {
                        m_albedo_output->GetRenderTargetFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_UAV
                        },
                        GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        *m_normal_output,
                        {
                        m_normal_output->GetRenderTargetFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_UAV
                        },
                        GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
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

    bool GBufferOutput::InitGBufferSRVs(glTFUniqueID pass_id,
                                        glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        *m_albedo_output,
                        {
                        m_albedo_output->GetRenderTargetFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_SRV
                        },
                        GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        *m_normal_output,
                        {
                        m_normal_output->GetRenderTargetFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_SRV
                        },
                        GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
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
        m_albedo_output->Transition(command_list, after);
        m_normal_output->Transition(command_list, after);
        m_depth_output->Transition(command_list, after);
        
        return true;
    }

    bool GBufferOutput::Bind(glTFUniqueID pass_id, RHIPipelineType pipeline_type, IRHICommandList& command_list, IRHIDescriptorUpdater& updater, const GBufferSignatureAllocationWithinPass& allocation) const
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_albedo_allocation.parameter_index, *GBufferPassResource.m_albedo_handle);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_normal_allocation.parameter_index, *GBufferPassResource.m_normal_handle);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_depth_allocation.parameter_index, *GBufferPassResource.m_depth_handle);
        
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
    , m_writable_buffer_handle(0)
    , m_back_buffer_handle(0)
    {
    }

    bool RWTextureResourceWithBackBuffer::CreateResource(glTFRenderResourceManager& resource_manager, const RHITextureDesc& desc)
    {
        m_texture0_desc = desc;
        m_texture1_desc = desc;

        m_texture0_desc.SetName(m_texture0_desc.GetName() + "_0");
        m_texture1_desc.SetName(m_texture0_desc.GetName() + "_1");
        
        auto format = m_texture0_desc.GetClearValue().clear_format;
    
        m_writable_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager,
            m_texture0_desc, {
                .type = RHIRenderTargetType::RTV,
                .format = RHIDataFormat::UNKNOWN
            });

        m_back_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager,
            m_texture0_desc, {
                .type = RHIRenderTargetType::RTV,
                .format = RHIDataFormat::UNKNOWN
            });
        
        return true;
    }

    bool RWTextureResourceWithBackBuffer::CreateDescriptors(glTFRenderResourceManager& resource_manager)
    {
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), *m_writable_buffer,
                                {m_writable_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV}, m_writable_buffer_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), *m_back_buffer,
                                {m_back_buffer->GetRenderTargetFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_back_buffer_handle))

        return true;
    }

    bool RWTextureResourceWithBackBuffer::RegisterSignature(IRHIRootSignatureHelper& root_signature)
    {
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetOutputBufferResourceName(), RHIRootParameterDescriptorRangeType::UAV, 1, false, m_writable_buffer_allocation))
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(GetBackBufferResourceName(), RHIRootParameterDescriptorRangeType::SRV, 1, false, m_back_buffer_allocation))
    
        return true;
    }

    bool RWTextureResourceWithBackBuffer::PostRendering(glTFRenderResourceManager& resource_manager)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
    
        // Copy accumulation buffer to back buffer
        m_writable_buffer->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
        m_back_buffer->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
        
        RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list, m_back_buffer->GetTexture(), m_writable_buffer->GetTexture()))

        return true;
    }

    std::string RWTextureResourceWithBackBuffer::GetOutputBufferResourceName() const
    {
        return m_texture0_desc.GetName();
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

    bool RWTextureResourceWithBackBuffer::BindDescriptors(IRHICommandList& command_list, RHIPipelineType pipeline_type,
        IRHIDescriptorUpdater& updater)
    {
        PreRendering(command_list);
        updater.BindDescriptor(command_list, pipeline_type, m_writable_buffer_allocation.parameter_index, *m_writable_buffer_handle);
        updater.BindDescriptor(command_list, pipeline_type, m_back_buffer_allocation.parameter_index, *m_back_buffer_handle);
        
        return true;
    }

    bool RWTextureResourceWithBackBuffer::PreRendering(IRHICommandList& command_list)
    {
        m_writable_buffer->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
        m_back_buffer->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
        
        return true;
    }
}

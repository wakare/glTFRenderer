#include "glTFRenderResourceUtils.h"

#include "glTFRenderResourceManager.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIDescriptorUpdater.h"
#include "RHIInterface/IRHIRenderTargetManager.h"

namespace glTFRenderResourceUtils
{
    bool GBufferSignatureAllocationWithinPass::InitGBufferAllocation(RendererUniqueObjectID pass_id,
        IRHIRootSignatureHelper& root_signature_helper, bool asSRV)
    {
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("ALBEDO_REGISTER_INDEX", {asSRV ? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV, 1, false, false}, m_albedo_allocation))
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("NORMAL_REGISTER_INDEX", {asSRV ? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV, 1, false, false}, m_normal_allocation))
        RETURN_IF_FALSE(root_signature_helper.AddTableRootParameter("DEPTH_REGISTER_INDEX", {asSRV ? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV, 1, false, false}, m_depth_allocation))
    
        return true;
    }

    bool GBufferSignatureAllocationWithinPass::UpdateShaderMacros(RendererUniqueObjectID pass_id,
                                                                  RHIShaderPreDefineMacros& macros, bool asSRV) const
    {
        m_albedo_allocation.AddShaderDefine(macros);
        m_normal_allocation.AddShaderDefine(macros);
        m_depth_allocation.AddShaderDefine(macros);
        return true;
    }

    GBufferSignatureAllocationWithinPass& GBufferSignatureAllocations::GetAllocationWithPassId(RendererUniqueObjectID pass_id)
    {
        return m_allocations[pass_id];
    }

    bool GBufferSignatureAllocations::InitGBufferAllocation(RendererUniqueObjectID pass_id,
                                                           IRHIRootSignatureHelper& root_signature_helper, bool asSRV)
    {
        return m_allocations[pass_id].InitGBufferAllocation(pass_id, root_signature_helper, asSRV);
    }

    bool GBufferSignatureAllocations::UpdateShaderMacros(RendererUniqueObjectID pass_id, RHIShaderPreDefineMacros& macros,
        bool asSRV) const
    {
        const auto find_it = m_allocations.find(pass_id);
        GLTF_CHECK(find_it!= m_allocations.end());
        return find_it->second.UpdateShaderMacros(pass_id, macros, asSRV);
    }

    bool GBufferOutput::InitGBufferOutput(glTFRenderResourceManager& resource_manager, unsigned back_buffer_index)
    {
        const unsigned width  = resource_manager.GetSwapChain().GetWidth();
        const unsigned height = resource_manager.GetSwapChain().GetHeight();
        
        m_albedo_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(),
            resource_manager.GetMemoryManager(),
            RHITextureDesc::MakeBasePassAlbedoTextureDesc(width, height));
        
        m_normal_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(),
            resource_manager.GetMemoryManager(),
            RHITextureDesc::MakeBasePassNormalTextureDesc(width, height));
        
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
            resource_manager.GetDevice(), resource_manager.GetMemoryManager(),
            depth_output_desc, RHIDataFormat::D32_FLOAT);
        
        return true;
    }

    bool GBufferOutput::InitGBufferUAVs(RendererUniqueObjectID pass_id, glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_albedo_output->m_source,
                        {
                        m_albedo_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_UAV
                        },
                        GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_normal_output->m_source,
                        {
                        m_normal_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_UAV
                        },
                        GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_depth_output->m_source,
                        {
                        m_depth_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_UAV
                        },
                        GBufferPassResource.m_depth_handle))

        return true;
    }

    bool GBufferOutput::InitGBufferSRVs(RendererUniqueObjectID pass_id,
                                        glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_albedo_output->m_source,
                        {
                        m_albedo_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_SRV
                        },
                        GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_normal_output->m_source,
                        {
                        m_normal_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_SRV
                        },
                        GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(
                        resource_manager.GetDevice(),
                        m_depth_output->m_source,
                        {
                        m_depth_output->GetDesc().m_format,
                        RHIResourceDimension::TEXTURE2D,
                        RHIViewType::RVT_SRV
                        },
                        GBufferPassResource.m_depth_handle))

        return true;
    }

    bool GBufferOutput::Transition(RendererUniqueObjectID pass_id,IRHICommandList& command_list, RHIResourceStateType after) const
    {
        m_albedo_output->m_source->Transition(command_list, after);
        m_normal_output->m_source->Transition(command_list, after);
        m_depth_output->m_source->Transition(command_list, after);
        
        return true;
    }

    bool GBufferOutput::Bind(RendererUniqueObjectID pass_id, RHIPipelineType pipeline_type, IRHICommandList& command_list, IRHIDescriptorUpdater& updater, const GBufferSignatureAllocationWithinPass& allocation) const
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_albedo_allocation, *GBufferPassResource.m_albedo_handle);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_normal_allocation, *GBufferPassResource.m_normal_handle);
        updater.BindDescriptor(command_list, pipeline_type, allocation.m_depth_allocation, *GBufferPassResource.m_depth_handle);
        
        return true;
    }

    GBufferResourceWithinPass& GBufferOutput::GetGBufferPassResource(RendererUniqueObjectID id)
    {
        return m_GBuffer_pass_resource[id];
    }

    const GBufferResourceWithinPass& GBufferOutput::GetGBufferPassResource(RendererUniqueObjectID id) const
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
            resource_manager.GetDevice(), resource_manager.GetMemoryManager(),
            m_texture0_desc);

        m_back_buffer = resource_manager.GetRenderTargetManager().CreateRenderTarget(
            resource_manager.GetDevice(), resource_manager.GetMemoryManager(),
            m_texture0_desc);
        
        return true;
    }

    bool RWTextureResourceWithBackBuffer::CreateDescriptors(glTFRenderResourceManager& resource_manager)
    {
        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_writable_buffer->m_source,
                                {m_writable_buffer->GetDesc().m_format, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV}, m_writable_buffer_handle))

        RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_back_buffer->m_source,
                                {m_back_buffer->GetDesc().m_format, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_back_buffer_handle))

        return true;
    }

    bool RWTextureResourceWithBackBuffer::RegisterSignature(IRHIRootSignatureHelper& root_signature)
    {
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(m_output_register_name, {RHIDescriptorRangeType::UAV, 1, false, false}, m_writable_buffer_allocation))
        RETURN_IF_FALSE(root_signature.AddTableRootParameter(m_back_register_name, {RHIDescriptorRangeType::SRV, 1, false, false}, m_back_buffer_allocation))
    
        return true;
    }

    bool RWTextureResourceWithBackBuffer::PostRendering(glTFRenderResourceManager& resource_manager)
    {
        auto& command_list = resource_manager.GetCommandListForRecord();
    
        // Copy accumulation buffer to back buffer
        m_writable_buffer->m_source->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
        m_back_buffer->m_source->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
        
        RETURN_IF_FALSE(RHIUtilInstanceManager::Instance().CopyTexture(command_list, *m_back_buffer->m_source, *m_writable_buffer->m_source, {}))

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
        m_writable_buffer_allocation.AddShaderDefine(macros);
        m_back_buffer_allocation.AddShaderDefine(macros);
        
        return true;
    }

    bool RWTextureResourceWithBackBuffer::BindDescriptors(IRHICommandList& command_list, RHIPipelineType pipeline_type,
        IRHIDescriptorUpdater& updater)
    {
        PreRendering(command_list);
        updater.BindDescriptor(command_list, pipeline_type, m_writable_buffer_allocation, *m_writable_buffer_handle);
        updater.BindDescriptor(command_list, pipeline_type, m_back_buffer_allocation, *m_back_buffer_handle);
        
        return true;
    }

    bool RWTextureResourceWithBackBuffer::PreRendering(IRHICommandList& command_list)
    {
        m_writable_buffer->m_source->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
        m_back_buffer->m_source->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
        
        return true;
    }
}

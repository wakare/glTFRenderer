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
        IRHIRenderTargetDesc albedo_output_desc;
        albedo_output_desc.width = resource_manager.GetSwapchain().GetWidth();
        albedo_output_desc.height = resource_manager.GetSwapchain().GetHeight();
        albedo_output_desc.name = "Albedo_Output";
        albedo_output_desc.isUAV = true;
        albedo_output_desc.clearValue.clear_format = albedo_format;
        albedo_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

        m_albedo_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, albedo_format, albedo_format, albedo_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Albedo_Output", m_albedo_output, back_buffer_index);
        
        RHIDataFormat normal_format = RHIDataFormat::R8G8B8A8_UNORM;
        IRHIRenderTargetDesc normal_output_desc;
        normal_output_desc.width = resource_manager.GetSwapchain().GetWidth();
        normal_output_desc.height = resource_manager.GetSwapchain().GetHeight();
        normal_output_desc.name = "Normal_Output";
        normal_output_desc.isUAV = true;
        normal_output_desc.clearValue.clear_format = normal_format;
        normal_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

        m_normal_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, normal_format, normal_format, normal_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Normal_Output", m_normal_output, back_buffer_index);
        
        RHIDataFormat depth_format = RHIDataFormat::R32_FLOAT;
        IRHIRenderTargetDesc depth_output_desc;
        depth_output_desc.width = resource_manager.GetSwapchain().GetWidth();
        depth_output_desc.height = resource_manager.GetSwapchain().GetHeight();
        depth_output_desc.name = "Depth_Output";
        depth_output_desc.isUAV = true;
        depth_output_desc.clearValue.clear_format = depth_format;
        depth_output_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

        m_depth_output = resource_manager.GetRenderTargetManager().CreateRenderTarget(
                    resource_manager.GetDevice(), RHIRenderTargetType::RTV, depth_format, depth_format, depth_output_desc);
        resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("Depth_Output", m_depth_output, back_buffer_index);
        
        m_resource_state = RHIResourceStateType::STATE_RENDER_TARGET;
        
        return true;
    }

    bool GBufferOutput::InitGBufferUAVs(glTFUniqueID pass_id, IRHIDescriptorHeap& heap, glTFRenderResourceManager& resource_manager)
    {
        auto& GBufferPassResource = GetGBufferPassResource(pass_id);
        
        RETURN_IF_FALSE(heap.CreateUnOrderAccessViewInDescriptorHeap(
            resource_manager.GetDevice(),
            heap.GetUsedDescriptorCount(),
            *m_albedo_output,
            {
                m_albedo_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
            },
            GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(heap.CreateUnOrderAccessViewInDescriptorHeap(
            resource_manager.GetDevice(),
            heap.GetUsedDescriptorCount(),
            *m_normal_output,
            {
                m_normal_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
            },
            GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(heap.CreateUnOrderAccessViewInDescriptorHeap(
            resource_manager.GetDevice(),
            heap.GetUsedDescriptorCount(),
            *m_depth_output,
            {
                m_depth_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
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
            heap.GetUsedDescriptorCount(),
            *m_albedo_output,
            {
                m_albedo_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
            },
            GBufferPassResource.m_albedo_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
            resource_manager.GetDevice(),
            heap.GetUsedDescriptorCount(),
            *m_normal_output,
            {
                m_normal_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
            },
            GBufferPassResource.m_normal_handle))

        RETURN_IF_FALSE(heap.CreateShaderResourceViewInDescriptorHeap(
            resource_manager.GetDevice(),
            heap.GetUsedDescriptorCount(),
            *m_depth_output,
            {
                m_depth_output->GetRenderTargetFormat(),
                RHIResourceDimension::TEXTURE2D
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
}

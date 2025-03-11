#include "glTFRenderInterfaceVT.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceTextureTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

glTFRenderInterfaceVT::glTFRenderInterfaceVT(bool feed_back)
    : m_feed_back(feed_back)
{
    if (m_feed_back)
    {
        AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>(VTLogicalTextureInfo::Name.c_str()));
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIRootParameterDescriptorRangeType::UAV>>("VT_FEED_BACK_TEXTURE_REGISTER_INDEX"));
    }
    else
    {
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIRootParameterDescriptorRangeType::SRV>>("VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTable<1, RHIRootParameterDescriptorRangeType::SRV>>("VT_PHYSICAL_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Point>>("VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX"));
    }
}

bool glTFRenderInterfaceVT::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderInterfaceBase::PostInitInterfaceImpl(resource_manager))

    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    m_physical_texture = vt_system->GetPhysicalTexture()->GetTextureAllocation()->m_texture;
    if (m_feed_back)
    {
        std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> vt_page_table_allocations;
        std::vector<VTLogicalTextureInfo> vt_logical_texture_infos;
        for (const auto& page_table : vt_system->GetLogicalTextureInfos())
        {
            std::shared_ptr<IRHITextureDescriptorAllocation> result;
            const auto& logical_texture = page_table.second.first;
            auto& texture_resource = logical_texture->GetTextureAllocation()->m_texture;
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture_resource,
                RHITextureDescriptorDesc{
                    texture_resource->GetTextureDesc().GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_UAV,
                },
                result);
            vt_page_table_allocations.push_back(result);

            VTLogicalTextureInfo info;
            info.logical_texture_output_index = logical_texture->GetTextureId();
            info.logical_texture_size = logical_texture->GetSize();
            vt_logical_texture_infos.push_back(info);
        }
        GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIRootParameterDescriptorRangeType::UAV>>()->AddTextureAllocations(vt_page_table_allocations);

        GetRenderInterface<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>()->UploadCPUBuffer(resource_manager, vt_logical_texture_infos.data(), 0, sizeof(vt_logical_texture_infos) * sizeof(VTLogicalTextureInfo));
    }
    else
    {
        std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> vt_page_table_allocations;
        for (const auto& page_table : vt_system->GetLogicalTextureInfos())
        {
            std::shared_ptr<IRHITextureDescriptorAllocation> result;
            auto& texture_resource = page_table.second.second->GetTextureAllocation()->m_texture;
            resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture_resource,
                RHITextureDescriptorDesc{
                    texture_resource->GetTextureDesc().GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV,
                },
                result);
            vt_page_table_allocations.push_back(result);
        }
        GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIRootParameterDescriptorRangeType::SRV>>()->AddTextureAllocations(vt_page_table_allocations);

        std::shared_ptr<IRHITextureDescriptorAllocation> vt_physical_descriptor_allocation;
        resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), m_physical_texture,
                RHITextureDescriptorDesc{
                    m_physical_texture->GetTextureDesc().GetDataFormat(),
                    RHIResourceDimension::TEXTURE2D,
                    RHIViewType::RVT_SRV,
                },
                vt_physical_descriptor_allocation);
        GetRenderInterface<glTFRenderInterfaceTextureTable<1, RHIRootParameterDescriptorRangeType::SRV>>()->AddTextureAllocations({vt_physical_descriptor_allocation});
    }
    
    return true;
}

bool glTFRenderInterfaceVT::ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type,
    IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index)
{
    RETURN_IF_FALSE(glTFRenderInterfaceBase::ApplyInterfaceImpl(command_list, pipeline_type, descriptor_updater, frame_index));
    if (m_feed_back)
    {
        
    }
    else
    {
        m_physical_texture->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);    
    }
    
    return true;
}

void glTFRenderInterfaceVT::ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const
{
    glTFRenderInterfaceBase::ApplyShaderDefineImpl(out_shader_pre_define_macros);

    if (m_feed_back)
    {
        out_shader_pre_define_macros.AddMacro("VT_FEED_BACK", "1");    
    }
    else
    {
        out_shader_pre_define_macros.AddMacro("VT_READ_DATA", "1");
    }
}

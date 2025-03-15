#include "glTFRenderInterfaceVT.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceTextureTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

glTFRenderInterfaceVT::glTFRenderInterfaceVT(bool feed_back)
    : m_feed_back(feed_back)
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>(VTLogicalTextureInfo::Name.c_str()));
    if (m_feed_back)
    {
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>("VT_FEED_BACK_TEXTURE_REGISTER_INDEX"));
    }
    else
    {
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>("VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTable<1, RHIDescriptorRangeType::SRV>>("VT_PHYSICAL_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Point>>("VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX"));
    }
}

bool glTFRenderInterfaceVT::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderInterfaceBase::PostInitInterfaceImpl(resource_manager))

    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    m_physical_texture = vt_system->GetPhysicalTexture()->GetTextureAllocation()->m_texture;
    
    std::vector<VTLogicalTextureInfo> vt_logical_texture_infos;
    for (const auto& page_table : vt_system->GetLogicalTextureInfos())
    {
        const auto& logical_texture = page_table.second.first;
        VTLogicalTextureInfo info;
        info.logical_texture_output_index = logical_texture->GetTextureId();
        info.logical_texture_size = logical_texture->GetSize();
        vt_logical_texture_infos.push_back(info);
    }
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>()->UploadBuffer(resource_manager, vt_logical_texture_infos.data(), 0,
        vt_logical_texture_infos.size() * sizeof(VTLogicalTextureInfo));
    
    if (m_feed_back)
    {
        for (const auto& page_table : vt_system->GetLogicalTextureInfos())
        {
            const auto& logical_texture = page_table.second.first;
            m_feedback_textures.push_back(logical_texture->GetTextureAllocation()->m_texture);
        }
        GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::UAV>>()->AddTexture(m_feedback_textures);
    }
    else
    {
        std::vector<std::shared_ptr<IRHITexture>> vt_page_table_textures;
        for (const auto& page_table : vt_system->GetLogicalTextureInfos())
        {
            auto& texture_resource = page_table.second.second->GetTextureAllocation()->m_texture;
            vt_page_table_textures.push_back(texture_resource);
        }
        GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>()->AddTexture(vt_page_table_textures);

        GetRenderInterface<glTFRenderInterfaceTextureTable<1, RHIDescriptorRangeType::SRV>>()->AddTexture({m_physical_texture});
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

#include "glTFRenderInterfaceVT.h"

#include "glTFRenderInterfaceSampler.h"
#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFRenderInterfaceTextureTable.h"
#include "glTFRenderInterfaceStructuredBuffer.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

glTFRenderInterfaceVT::glTFRenderInterfaceVT(InterfaceVTType type)
    : m_type(type)
{
    AddInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>());
    AddInterface(std::make_shared<glTFRenderInterfaceSingleConstantBuffer<VTSystemInfo>>());
    
    if (m_type == InterfaceVTType::SAMPLE_VT_TEXTURE_DATA)
    {
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>("VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceTextureTable<2, RHIDescriptorRangeType::SRV>>("VT_PHYSICAL_TEXTURE_REGISTER_INDEX"));
        AddInterface(std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Point>>("VT_PAGE_TABLE_SAMPLER_REGISTER_INDEX"));
    }
}

bool glTFRenderInterfaceVT::PreInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();
    m_physical_svt_texture = vt_system->GetSVTPhysicalTexture()->GetTextureAllocation()->m_texture;
    m_physical_rvt_texture = vt_system->GetRVTPhysicalTexture()->GetTextureAllocation()->m_texture;
    
    // Add reserved vt info!!
    m_vt_logical_texture_infos.resize(1);
    
    for (const auto& page_table : vt_system->GetLogicalTextureInfos())
    {
        const auto& logical_texture = page_table.second.first;
        VTLogicalTextureInfo info;
        info.logical_texture_size = logical_texture->GetSize();
        m_vt_logical_texture_infos.push_back(info);
    }
    
    if (m_type == InterfaceVTType::SAMPLE_VT_TEXTURE_DATA)
    {
        std::vector<std::shared_ptr<IRHITexture>> vt_page_table_textures;
        for (const auto& page_table : vt_system->GetLogicalTextureInfos())
        {
            const auto& logical_texture = page_table.second.first;
            auto& texture_resource = page_table.second.second->GetTextureAllocation()->m_texture;
            m_vt_logical_texture_infos[logical_texture->GetTextureId()].page_table_tex_index = vt_page_table_textures.size();
            m_vt_logical_texture_infos[logical_texture->GetTextureId()].page_table_texture_size = texture_resource->GetTextureDesc().GetTextureWidth();
            m_vt_logical_texture_infos[logical_texture->GetTextureId()].svt = logical_texture->IsSVT();
            vt_page_table_textures.push_back(texture_resource);
        }
        
        GetRenderInterface<glTFRenderInterfaceTextureTableBindless<RHIDescriptorRangeType::SRV>>()->AddTexture(vt_page_table_textures);
        GetRenderInterface<glTFRenderInterfaceTextureTable<2, RHIDescriptorRangeType::SRV>>()->AddTexture({m_physical_svt_texture, m_physical_rvt_texture});
    }
    
    return true;
}

bool glTFRenderInterfaceVT::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<VTLogicalTextureInfo>>()->UploadBuffer(resource_manager, m_vt_logical_texture_infos.data(), 0,
        m_vt_logical_texture_infos.size() * sizeof(VTLogicalTextureInfo));

    VTSystemInfo info{};
    info.border_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->VT_PHYSICAL_TEXTURE_BORDER;
    info.page_size = resource_manager.GetRenderSystem<VirtualTextureSystem>()->VT_PAGE_SIZE;
    info.physical_texture_width = resource_manager.GetRenderSystem<VirtualTextureSystem>()->VT_PHYSICAL_TEXTURE_SIZE;
    info.physical_texture_height = resource_manager.GetRenderSystem<VirtualTextureSystem>()->VT_PHYSICAL_TEXTURE_SIZE;
    GetRenderInterface<glTFRenderInterfaceSingleConstantBuffer<VTSystemInfo>>()->UploadBuffer(resource_manager, &info, 0, sizeof(info));
    
    return true;
    
}

void glTFRenderInterfaceVT::ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros)
{
    glTFRenderInterfaceBase::ApplyShaderDefineImpl(out_shader_pre_define_macros);

    if (m_type == InterfaceVTType::SAMPLE_VT_TEXTURE_DATA)
    {
        out_shader_pre_define_macros.AddMacro("VT_READ_DATA", "1");
    }
    else if (m_type == InterfaceVTType::RENDER_VT_FEEDBACK)
    {
        
    }
}

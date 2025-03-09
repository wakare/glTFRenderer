#include "glTFRenderInterfaceVT.h"

#include "glTFRenderInterfaceSRVTable.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"

glTFRenderInterfaceVT::glTFRenderInterfaceVT()
{
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>("VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX"));
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTable<1>>("VT_PHYSICAL_TEXTURE_REGISTER_INDEX"));
}

bool glTFRenderInterfaceVT::AddVirtualTexture(std::shared_ptr<VTLogicalTexture> virtual_texture)
{
    m_virtual_textures.push_back(virtual_texture);
    return true;
}

bool glTFRenderInterfaceVT::PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderInterfaceBase::PostInitInterfaceImpl(resource_manager));

    auto vt_system = resource_manager.GetRenderSystem<VirtualTextureSystem>();

    std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> vt_page_table_allocations;
    for (const auto& page_table :  vt_system->GetLogicalTextureInfos())
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
    GetRenderInterface<glTFRenderInterfaceSRVTableBindless>()->AddTextureAllocations(vt_page_table_allocations);

    std::shared_ptr<IRHITextureDescriptorAllocation> vt_physical_descriptor_allocation;
    auto& texture_resource = vt_system->GetPhysicalTexture()->GetTextureAllocation()->m_texture;
    std::shared_ptr<IRHITextureDescriptorAllocation> result;
    resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), texture_resource,
            RHITextureDescriptorDesc{
                texture_resource->GetTextureDesc().GetDataFormat(),
                RHIResourceDimension::TEXTURE2D,
                RHIViewType::RVT_SRV,
            },
            result);
    GetRenderInterface<glTFRenderInterfaceSRVTable<1>>()->AddTextureAllocations({result});
    
    return true;
}

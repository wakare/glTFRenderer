#include "glTFRenderInterfaceVT.h"

#include "glTFRenderInterfaceSRVTable.h"

glTFRenderInterfaceVT::glTFRenderInterfaceVT()
{
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTableBindless>("VT_PAGE_TABLE_TEXTURE_REGISTER_INDEX"));
    AddInterface(std::make_shared<glTFRenderInterfaceSRVTable<1>>("VT_PHYSICAL_TEXTURE_REGISTER_INDEX"));
}

bool glTFRenderInterfaceVT::AddVirtualTexture(std::shared_ptr<VTLogicalTexture> virtual_texture)
{
    return true;
}

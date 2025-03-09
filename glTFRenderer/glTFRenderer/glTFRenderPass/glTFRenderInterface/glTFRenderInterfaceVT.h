#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

class glTFRenderInterfaceVT : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceVT();
    bool AddVirtualTexture(std::shared_ptr<VTLogicalTexture> virtual_texture);
    
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    
protected:
    std::vector<std::shared_ptr<VTLogicalTexture>> m_virtual_textures;
};

#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

class glTFRenderInterfaceVT : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceVT();
    bool AddVirtualTexture(std::shared_ptr<VTLogicalTexture> virtual_texture);
    
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override;
protected:
    std::vector<std::shared_ptr<VTLogicalTexture>> m_virtual_textures;
    std::shared_ptr<IRHITexture> m_physical_texture;
};
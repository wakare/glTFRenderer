#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFRenderSystem/VT/VTTextureTypes.h"

class glTFRenderInterfaceVT : public glTFRenderInterfaceBase
{
public:
    glTFRenderInterfaceVT(bool feed_back);
    
    virtual bool PostInitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override;
protected:
    virtual void ApplyShaderDefineImpl(RHIShaderPreDefineMacros& out_shader_pre_define_macros) const override;
    
    bool m_feed_back;
    std::shared_ptr<IRHITexture> m_physical_texture;
};
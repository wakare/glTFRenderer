#pragma once
#include "glTFRenderInterfaceBase.h"
#include "glTFScene/glTFSceneView.h"

class glTFRenderInterfaceShadowmap : public glTFRenderInterfaceBase
{
public:
    enum class ShadowmapType
    {
        DRAW_SHADOW_MAP,
        READ_SHADOW_MAP,
    };

    glTFRenderInterfaceShadowmap(ShadowmapType type);

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index) override;
    
protected:
    ShadowmapType m_type;
};

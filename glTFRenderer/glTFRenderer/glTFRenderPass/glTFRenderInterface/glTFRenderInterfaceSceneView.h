#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceSceneView();

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override;
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override;
    
protected:
    std::vector<std::shared_ptr<IRHIBufferDescriptorAllocation>> m_scene_view_buffer_allocations;
};
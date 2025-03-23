#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"

class glTFRenderInterfaceViewBase : public glTFRenderInterfaceWithRSAllocation
{
public:
    glTFRenderInterfaceViewBase(const char* name);

    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    virtual bool ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned
                                    frame_index) override;
    virtual bool ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature) override;
protected:
    virtual std::vector<std::shared_ptr<IRHIBufferAllocation>> GetViewBufferAllocation(glTFRenderResourceManager& resource_manager) = 0;
    std::vector<std::shared_ptr<IRHIBufferDescriptorAllocation>> m_scene_view_buffer_allocations;
};

class glTFRenderInterfaceSceneView : public glTFRenderInterfaceViewBase
{
public:
    glTFRenderInterfaceSceneView();

protected:
    virtual std::vector<std::shared_ptr<IRHIBufferAllocation>> GetViewBufferAllocation(glTFRenderResourceManager& resource_manager) override;
};

class glTFRenderInterfaceShadowMapView : public glTFRenderInterfaceViewBase
{
public:
    glTFRenderInterfaceShadowMapView(int light_id);

protected:
    virtual std::vector<std::shared_ptr<IRHIBufferAllocation>> GetViewBufferAllocation(glTFRenderResourceManager& resource_manager) override;

    int m_light_id{-1};
};

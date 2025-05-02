#pragma once

#include "glTFRenderInterfaceSingleConstantBuffer.h"
#include "glTFLight/glTFDirectionalLight.h"

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

class glTFRenderInterfaceSharedShadowMapView : public glTFRenderInterfaceViewBase
{
public:
    glTFRenderInterfaceSharedShadowMapView(int light_id);

protected:
    virtual std::vector<std::shared_ptr<IRHIBufferAllocation>> GetViewBufferAllocation(glTFRenderResourceManager& resource_manager) override;

    int m_light_id{-1};
};

class glTFRenderInterfaceVirtualShadowMapView : public glTFRenderInterfaceViewBase
{
public:
    glTFRenderInterfaceVirtualShadowMapView(int light_id);
    virtual bool InitInterfaceImpl(glTFRenderResourceManager& resource_manager) override;
    void SetNDCRange(float min_x, float min_y, float width, float height);
    void CalculateShadowmapMatrix(glTFRenderResourceManager& resource_manager, const glTFDirectionalLight& directional_light, const glTF_AABB::AABB& scene_bounds);
    
protected:
    virtual std::vector<std::shared_ptr<IRHIBufferAllocation>> GetViewBufferAllocation(glTFRenderResourceManager& resource_manager) override;

    std::vector<std::shared_ptr<IRHIBufferAllocation>> m_virtual_shadowmap_buffer_allocations;
    
    int m_light_id{-1};
    float m_min_x{0.0f};
    float m_min_y{0.0f};
    float m_width{0.0f};
    float m_height{0.0f};
};
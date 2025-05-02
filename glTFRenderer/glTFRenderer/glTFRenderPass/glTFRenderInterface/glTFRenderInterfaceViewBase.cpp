#include "glTFRenderInterfaceViewBase.h"

const static char* SceneViewBufferName = "SCENE_VIEW_REGISTER_INDEX";

glTFRenderInterfaceViewBase::glTFRenderInterfaceViewBase(const char* name)
    : glTFRenderInterfaceWithRSAllocation(name)
{
}

bool glTFRenderInterfaceViewBase::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    m_scene_view_buffer_allocations.resize(resource_manager.GetBackBufferCount());
    const auto view_buffers = GetViewBufferAllocation(resource_manager);
    for (unsigned i = 0; i < m_scene_view_buffer_allocations.size(); ++i)
    {
        auto& buffer_allocation = m_scene_view_buffer_allocations[i];
        const auto& scene_view_buffer = view_buffers[i];
        
        resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), scene_view_buffer->m_buffer, RHIBufferDescriptorDesc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_CBV,
                static_cast<unsigned>(scene_view_buffer->m_buffer->GetBufferDesc().width),
                0
            }, buffer_allocation);
    }
    
    return true;
}

bool glTFRenderInterfaceViewBase::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager, IRHICommandList& command_list,
                                                      RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index)
{
    descriptor_updater.BindTextureDescriptorTable(command_list, pipeline_type, m_allocation, *m_scene_view_buffer_allocations[frame_index % m_scene_view_buffer_allocations.size() ]);
    return true;
    
}

bool glTFRenderInterfaceViewBase::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
    return root_signature.AddCBVRootParameter(SceneViewBufferName, m_allocation);
}

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView()
    : glTFRenderInterfaceViewBase(SceneViewBufferName)
{
    
}

std::vector<std::shared_ptr<IRHIBufferAllocation>> glTFRenderInterfaceSceneView::GetViewBufferAllocation(glTFRenderResourceManager& resource_manager)
{
    std::vector<std::shared_ptr<IRHIBufferAllocation>> result; result.resize(resource_manager.GetBackBufferCount());
    
    for (unsigned i = 0; i < result.size(); ++i)
    {
        result[i] = resource_manager.GetPerFrameRenderResourceData()[i].GetSceneViewBufferAllocation();
    }
    
    return result;
}

glTFRenderInterfaceSharedShadowMapView::glTFRenderInterfaceSharedShadowMapView(int light_id)
    : glTFRenderInterfaceViewBase(SceneViewBufferName)
    , m_light_id(light_id)
{
    GLTF_CHECK(light_id >= 0);
}

std::vector<std::shared_ptr<IRHIBufferAllocation>> glTFRenderInterfaceSharedShadowMapView::GetViewBufferAllocation(
    glTFRenderResourceManager& resource_manager)
{
    std::vector<std::shared_ptr<IRHIBufferAllocation>> result; result.resize(resource_manager.GetBackBufferCount());
    
    for (unsigned i = 0; i < result.size(); ++i)
    {
        result[i] = resource_manager.GetPerFrameRenderResourceData()[i].GetShadowmapViewBufferAllocation(m_light_id);
    }
    
    return result;
}

glTFRenderInterfaceVirtualShadowMapView::glTFRenderInterfaceVirtualShadowMapView(int light_id)
    : glTFRenderInterfaceViewBase(SceneViewBufferName)
    , m_light_id(light_id)
{
    
}

bool glTFRenderInterfaceVirtualShadowMapView::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderInterfaceViewBase::InitInterfaceImpl(resource_manager))

    return true;
}

void glTFRenderInterfaceVirtualShadowMapView::SetNDCRange(float min_x, float min_y, float width, float height)
{
    GLTF_CHECK(min_x >= 0.0f && min_y >= 0.0f && width > 0.0f && height > 0.0f);
    
    m_min_x = min_x;
    m_min_y = min_y;
    m_width = width;
    m_height = height;
}

void glTFRenderInterfaceVirtualShadowMapView::CalculateShadowmapMatrix(glTFRenderResourceManager& resource_manager, const glTFDirectionalLight& directional_light, const glTF_AABB::AABB& scene_bounds)
{
    LightShadowmapViewRange range;
    range.ndc_min_x = m_min_x;
    range.ndc_min_y = m_min_y;
    range.ndc_width = m_width;
    range.ndc_height = m_height;
    
    auto shadowmap_view_info = directional_light.GetShadowmapViewInfo(scene_bounds, range);
    m_current_shadowmap_view.view_position = shadowmap_view_info.position;
    m_current_shadowmap_view.view_matrix = shadowmap_view_info.view_matrix;
    m_current_shadowmap_view.inverse_view_matrix = glm::inverse(shadowmap_view_info.view_matrix);
    m_current_shadowmap_view.projection_matrix = shadowmap_view_info.projection_matrix;
    m_current_shadowmap_view.inverse_projection_matrix = glm::inverse(shadowmap_view_info.projection_matrix);
}

bool glTFRenderInterfaceVirtualShadowMapView::ApplyInterfaceImpl(glTFRenderResourceManager& resource_manager,
    IRHICommandList& command_list, RHIPipelineType pipeline_type, IRHIDescriptorUpdater& descriptor_updater,
    unsigned frame_index)
{    
    resource_manager.GetMemoryManager().UploadBufferData(*m_virtual_shadowmap_buffer_allocations[resource_manager.GetCurrentBackBufferIndex()], &m_current_shadowmap_view, 0, sizeof(m_current_shadowmap_view));

    RETURN_IF_FALSE(glTFRenderInterfaceViewBase::ApplyInterfaceImpl(resource_manager, command_list, pipeline_type,
                                                           descriptor_updater, frame_index))

    return true;
}

std::vector<std::shared_ptr<IRHIBufferAllocation>> glTFRenderInterfaceVirtualShadowMapView::GetViewBufferAllocation(
    glTFRenderResourceManager& resource_manager)
{
    if (m_virtual_shadowmap_buffer_allocations.empty())
    {
        m_virtual_shadowmap_buffer_allocations.resize(resource_manager.GetBackBufferCount());
        auto& memory_manager = resource_manager.GetMemoryManager();

        RHIBufferDesc virtual_shadowmap_buffer_desc;
        virtual_shadowmap_buffer_desc.name = L"VirtualShadowmapBuffer";
        virtual_shadowmap_buffer_desc.width = 64ull * 1024;
        virtual_shadowmap_buffer_desc.height = 1;
        virtual_shadowmap_buffer_desc.depth = 1;
        virtual_shadowmap_buffer_desc.type = RHIBufferType::Upload;
        virtual_shadowmap_buffer_desc.resource_type = RHIBufferResourceType::Buffer;
        virtual_shadowmap_buffer_desc.resource_data_type = RHIDataFormat::UNKNOWN;
        virtual_shadowmap_buffer_desc.state = RHIResourceStateType::STATE_COMMON;
        virtual_shadowmap_buffer_desc.usage = RHIResourceUsageFlags::RUF_ALLOW_CBV;
        for (int i = 0; i < m_virtual_shadowmap_buffer_allocations.size(); ++i)
        {
            memory_manager.AllocateBufferMemory(resource_manager.GetDevice(), virtual_shadowmap_buffer_desc, m_virtual_shadowmap_buffer_allocations[i]);
        }
    }
    
    return m_virtual_shadowmap_buffer_allocations;
}

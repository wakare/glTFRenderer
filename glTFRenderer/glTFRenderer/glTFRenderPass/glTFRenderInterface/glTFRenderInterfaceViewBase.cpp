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

glTFRenderInterfaceShadowMapView::glTFRenderInterfaceShadowMapView(int light_id)
    : glTFRenderInterfaceViewBase(SceneViewBufferName)
    , m_light_id(light_id)
{
    GLTF_CHECK(light_id >= 0);
}

std::vector<std::shared_ptr<IRHIBufferAllocation>> glTFRenderInterfaceShadowMapView::GetViewBufferAllocation(
    glTFRenderResourceManager& resource_manager)
{
    std::vector<std::shared_ptr<IRHIBufferAllocation>> result; result.resize(resource_manager.GetBackBufferCount());
    
    for (unsigned i = 0; i < result.size(); ++i)
    {
        result[i] = resource_manager.GetPerFrameRenderResourceData()[i].GetShadowmapViewBufferAllocation(m_light_id);
    }
    
    return result;
}
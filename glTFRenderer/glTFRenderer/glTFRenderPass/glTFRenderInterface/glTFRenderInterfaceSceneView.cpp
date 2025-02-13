#include "glTFRenderInterfaceSceneView.h"

#include "glTFScene/glTFSceneView.h"

const static char* SceneViewBufferName = "SCENE_VIEW_REGISTER_INDEX";

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView()
    : glTFRenderInterfaceWithRSAllocation(SceneViewBufferName)
{
}

bool glTFRenderInterfaceSceneView::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    m_scene_view_buffer_allocations.resize(resource_manager.GetBackBufferCount());
    for (unsigned i = 0; i < m_scene_view_buffer_allocations.size(); ++i)
    {
        auto& buffer_allocation = m_scene_view_buffer_allocations[i];
        const auto& scene_view_buffer = resource_manager.GetPerFrameRenderResourceData()[i].m_scene_view_buffer;
        buffer_allocation = CreateBufferDescriptor();
        buffer_allocation->InitFromBuffer(scene_view_buffer->m_buffer,
            RHIBufferDescriptorDesc{
                RHIDataFormat::UNKNOWN,
                RHIViewType::RVT_CBV,
                static_cast<unsigned>(scene_view_buffer->m_buffer->GetBufferDesc().width),
                0
            });
    }
    
    return true;
}

bool glTFRenderInterfaceSceneView::ApplyInterfaceImpl(IRHICommandList& command_list, RHIPipelineType pipeline_type,
    IRHIDescriptorUpdater& descriptor_updater, unsigned frame_index)
{
    descriptor_updater.BindDescriptor(command_list, pipeline_type, m_allocation, *m_scene_view_buffer_allocations[frame_index % m_scene_view_buffer_allocations.size() ]);
    return true;
    
}

bool glTFRenderInterfaceSceneView::ApplyRootSignatureImpl(IRHIRootSignatureHelper& root_signature)
{
    return root_signature.AddCBVRootParameter(SceneViewBufferName, m_allocation);
}
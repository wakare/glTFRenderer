#include "glTFRenderInterfaceSceneView.h"

#include "glTFScene/glTFSceneView.h"

glTFRenderInterfaceSceneView::glTFRenderInterfaceSceneView()
    : glTFRenderInterfaceSingleConstantBuffer(ConstantBufferSceneView::Name.c_str())
{
}

bool glTFRenderInterfaceSceneView::InitInterfaceImpl(glTFRenderResourceManager& resource_manager)
{
    m_constant_gpu_data = resource_manager.GetPersistentData().m_scene_view_buffer;
    const bool create_buffer_descriptor = CreateConstantBufferDescriptor(m_constant_gpu_data, m_constant_buffer_descriptor_allocation);
    return create_buffer_descriptor;
}
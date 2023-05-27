#include "glTFRenderPassInterfaceSceneView.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactory.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"

bool glTFRenderPassInterfaceSceneView::InitInterface(glTFRenderResourceManager& resourceManager)
{
    m_sceneViewGPUData = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_sceneViewGPUData->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"SceneViewInterface_GPUBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }))
    
    return true;
}

bool glTFRenderPassInterfaceSceneView::UpdateSceneViewData(const ConstantBufferSceneView& data)
{
    m_sceneViewData = data;
    return true;
}

bool glTFRenderPassInterfaceSceneView::ApplyInterface(glTFRenderResourceManager& resourceManager,
                                                      unsigned rootParameterSlotIndex)
{
    RETURN_IF_FALSE(m_sceneViewGPUData->UploadBufferFromCPU(&m_sceneViewData, 0, sizeof(m_sceneViewData)))
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(),
        rootParameterSlotIndex, m_sceneViewGPUData->GetGPUBufferHandle()))
    
    return true;
}

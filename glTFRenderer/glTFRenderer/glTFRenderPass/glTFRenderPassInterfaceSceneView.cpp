#include "glTFRenderPassInterfaceSceneView.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactory.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassInterfaceSceneView::glTFRenderPassInterfaceSceneView(unsigned rootParameterIndex, unsigned registerIndex)
    : m_root_parameter_cbv_index(rootParameterIndex)
    , m_register_index(registerIndex)
{
    
}

bool glTFRenderPassInterfaceSceneView::InitInterface(glTFRenderResourceManager& resourceManager)
{
    m_scene_view_gpu_data = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    RETURN_IF_FALSE(m_scene_view_gpu_data->InitGPUBuffer(resourceManager.GetDevice(),
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
    m_scene_view_data = data;
    return true;
}

bool glTFRenderPassInterfaceSceneView::ApplyInterface(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(m_scene_view_gpu_data->UploadBufferFromCPU(&m_scene_view_data, 0, sizeof(m_scene_view_data)))
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        m_root_parameter_cbv_index, m_scene_view_gpu_data->GetGPUBufferHandle()))
    
    return true;
}

bool glTFRenderPassInterfaceSceneView::SetupRootSignature(IRHIRootSignature& rootSignature) const
{
    return rootSignature.GetRootParameter(m_root_parameter_cbv_index).InitAsCBV(m_register_index);
}

void glTFRenderPassInterfaceSceneView::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_VIEW_REGISTER_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_VIEW_REGISTER_INDEX", registerIndexValue);
}
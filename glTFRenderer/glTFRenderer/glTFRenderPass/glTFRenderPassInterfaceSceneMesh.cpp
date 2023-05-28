#include "glTFRenderPassInterfaceSceneMesh.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

const size_t SceneMeshGPUBufferMaxSize = 64 * 1024;

glTFRenderPassInterfaceSceneMesh::glTFRenderPassInterfaceSceneMesh(unsigned rootParameterIndex, unsigned registerIndex)
    : m_rootParameterIndex(rootParameterIndex)
    , m_registerIndex(registerIndex)
{
}

bool glTFRenderPassInterfaceSceneMesh::InitInterface(glTFRenderResourceManager& resourceManager)
{
    m_sceneMeshGPUData = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_sceneMeshGPUData->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"SceneMeshInterface_PerMeshConstantBuffer",
            static_cast<size_t>(SceneMeshGPUBufferMaxSize),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }))
    
    return true;
}

bool glTFRenderPassInterfaceSceneMesh::UpdateSceneMeshData(const ConstantBufferSceneMesh& data)
{
    m_sceneMeshData = data;
    return true;
}

bool glTFRenderPassInterfaceSceneMesh::ApplyInterface(glTFRenderResourceManager& resourceManager,
    unsigned meshIndex, unsigned rootParameterSlotIndex)
{
    // Constant buffer must be aligned with 256 bytes
    const size_t offsetAligned =  meshIndex * ((sizeof(m_sceneMeshData) + 255) & ~255);

    assert(offsetAligned < SceneMeshGPUBufferMaxSize);
    
    RETURN_IF_FALSE(m_sceneMeshGPUData->UploadBufferFromCPU(&m_sceneMeshData, offsetAligned, sizeof(m_sceneMeshData)))
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), rootParameterSlotIndex, m_sceneMeshGPUData->GetGPUBufferHandle() + offsetAligned))
    
    return true;
}

bool glTFRenderPassInterfaceSceneMesh::SetupRootSignature(IRHIRootSignature& rootSignature) const
{
    return rootSignature.GetRootParameter(m_rootParameterIndex).InitAsCBV(m_registerIndex);
}

void glTFRenderPassInterfaceSceneMesh::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_registerIndex);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_INDEX", registerIndexValue);
    LOG_FORMAT("[INFO] Add shader preDefine %s, %s\n", "SCENE_MESH_REGISTER_INDEX", registerIndexValue);
}

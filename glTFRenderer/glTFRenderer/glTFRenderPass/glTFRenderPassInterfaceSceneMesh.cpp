#include "glTFRenderPassInterfaceSceneMesh.h"
#include "../glTFRHI/RHIInterface/IRHIGPUBuffer.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"

const size_t SceneMeshGPUBufferMaxSize = 64 * 1024;

glTFRenderPassInterfaceSceneMesh::glTFRenderPassInterfaceSceneMesh(
    unsigned root_parameter_cbv_index,
    unsigned cbv_register_index,
    unsigned root_parameter_srv_index,
    unsigned srv_register_index,
    unsigned srv_max_count)
    : m_root_parameter_cbv_index(root_parameter_cbv_index)
    , m_cbv_register_index(cbv_register_index)
    , m_root_parameter_srv_index(root_parameter_srv_index)
    , m_srv_register_index(srv_register_index)
    , m_max_srv_count(srv_max_count)
{
}

bool glTFRenderPassInterfaceSceneMesh::InitInterface(glTFRenderResourceManager& resourceManager)
{
    m_scene_mesh_GPU_data = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_scene_mesh_GPU_data->InitGPUBuffer(resourceManager.GetDevice(),
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
    m_scene_mesh_data = data;
    return true;
}

bool glTFRenderPassInterfaceSceneMesh::ApplyInterface(glTFRenderResourceManager& resourceManager,
                                                      unsigned meshIndex)
{
    // Constant buffer must be aligned with 256 bytes
    const size_t offsetAligned = meshIndex * ((sizeof(m_scene_mesh_data) + 255) & ~255);

    assert(offsetAligned < SceneMeshGPUBufferMaxSize);
    
    RETURN_IF_FALSE(m_scene_mesh_GPU_data->UploadBufferFromCPU(&m_scene_mesh_data, offsetAligned, sizeof(m_scene_mesh_data)))
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandListForRecord(),
        m_root_parameter_cbv_index, m_scene_mesh_GPU_data->GetGPUBufferHandle() + offsetAligned))
    
    return true;
}

bool glTFRenderPassInterfaceSceneMesh::SetupRootSignature(IRHIRootSignature& rootSignature) const
{
    RETURN_IF_FALSE(rootSignature.GetRootParameter(m_root_parameter_cbv_index).InitAsCBV(m_cbv_register_index))

    const RHIRootParameterDescriptorRangeDesc srv_range_desc {RHIRootParameterDescriptorRangeType::SRV, m_srv_register_index, m_max_srv_count};
    RETURN_IF_FALSE(rootSignature.GetRootParameter(m_root_parameter_srv_index).InitAsDescriptorTableRange(1, &srv_range_desc))
    
    return true;
}

void glTFRenderPassInterfaceSceneMesh::UpdateShaderCompileDefine(RHIShaderPreDefineMacros& outShaderPreDefineMacros) const
{
    char registerIndexValue[16] = {'\0'};
    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(b%d)", m_cbv_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_CBV_INDEX", registerIndexValue);

    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_srv_register_index);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ZERO", registerIndexValue);

    (void)snprintf(registerIndexValue, sizeof(registerIndexValue), "register(t%d)", m_srv_register_index + 1);
    outShaderPreDefineMacros.AddMacro("SCENE_MESH_REGISTER_SRV_INDEX_ONE", registerIndexValue);
    
    LOG_FORMAT("[INFO] Add shader preDefine\n %s, %d\n %s %d\n",
        "SCENE_MESH_REGISTER_CBV_INDEX", m_cbv_register_index,
        "SCENE_MESH_REGISTER_SRV_INDEX_ZERO", m_srv_register_index)
}

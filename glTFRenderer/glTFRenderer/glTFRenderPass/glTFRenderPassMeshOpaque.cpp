#include "glTFRenderPassMeshOpaque.h"

#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

bool glTFRenderPassMeshOpaque::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::InitPass(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resource_manager.GetCommandList(), resource_manager.GetCurrentFrameCommandAllocator()))
    
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::InitInterface(resource_manager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneMesh::InitInterface(resource_manager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resource_manager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resource_manager.GetCommandList(),resource_manager.GetCommandQueue()))
    RETURN_IF_FALSE(resource_manager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resource_manager.GetCommandQueue()))

    LOG_FORMAT("[DEBUG] Init MeshPassOpaque finished!\n")
    return true;
}

bool glTFRenderPassMeshOpaque::RenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::RenderPass(resource_manager))
    
    return true;
}

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
    RETURN_IF_FALSE(material.GetMaterialType() == MaterialType::Opaque)
	RETURN_IF_FALSE(resource_manager.GetMaterialManager().InitMaterialRenderResource(resource_manager, *m_main_descriptor_heap, material))
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    // TODO: Calculate heap size
    return 128;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Init root signature
    constexpr size_t root_signature_parameter_count = MeshOpaquePass_RootParameter_LastIndex;
    constexpr size_t root_signature_static_sampler_count = 1;
    RETURN_IF_FALSE(m_root_signature->AllocateRootSignatureSpace(root_signature_parameter_count, root_signature_static_sampler_count))
    
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupRootSignature(resource_manager))
    
    const RHIRootParameterDescriptorRangeDesc srv_range_desc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_root_signature->GetRootParameter(MeshOpaquePass_RootParameter_MeshMaterialTexSRV).InitAsDescriptorTableRange(1, &srv_range_desc);
    
    m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_root_signature->InitRootSignature(resource_manager.GetDevice()))

    return true;
}

bool glTFRenderPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");
    
    RETURN_IF_FALSE (m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(), *m_root_signature, resource_manager.GetSwapchain()))

    return true;
}

bool glTFRenderPassMeshOpaque::BeginDrawMesh(glTFRenderResourceManager& resource_manager, glTFUniqueID meshID)
{
    // Using texture SRV slot when mesh material is texture
    glTFUniqueID material_ID = m_meshes[meshID].materialID;
    if (material_ID == glTFUniqueIDInvalid)
    {
        return true;
    }
    
	return resource_manager.ApplyMaterial(material_ID, MeshOpaquePass_RootParameter_MeshMaterialTexSRV);
}
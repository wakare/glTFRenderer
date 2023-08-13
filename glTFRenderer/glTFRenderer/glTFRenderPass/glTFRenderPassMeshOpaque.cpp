#include "glTFRenderPassMeshOpaque.h"

#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
    RETURN_IF_FALSE(material.GetMaterialType() == MaterialType::Opaque)
    // Material texture resource descriptor is alloc within current heap
	RETURN_IF_FALSE(resource_manager.GetMaterialManager().InitMaterialRenderResource(resource_manager, *m_main_descriptor_heap, material))
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    // TODO: Calculate heap size
    return 256;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    // Init root signature
    constexpr size_t root_signature_parameter_count = MeshBasePass_RootParameter_LastIndex;
    constexpr size_t root_signature_static_sampler_count = 1;
    
    RETURN_IF_FALSE(m_root_signature->AllocateRootSignatureSpace(root_signature_parameter_count, root_signature_static_sampler_count))
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupRootSignature(resource_manager))

    // TODO: Init sampler in material resource manager
    RETURN_IF_FALSE(m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear))
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
    glTFUniqueID material_ID = m_meshes[meshID].material_id;
    if (material_ID == glTFUniqueIDInvalid)
    {
        return true;
    }
    
	return resource_manager.ApplyMaterial(*m_main_descriptor_heap, material_ID, MeshBasePass_RootParameter_SceneMesh_SRV);
}
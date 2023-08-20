#include "glTFRenderPassMeshOpaque.h"

#include "glTFRenderMaterialManager.h"
#include "../glTFMaterial/glTFMaterialOpaque.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFRHI/RHIResourceFactoryImpl.hpp"
#include "../glTFRHI/RHIInterface/glTFImageLoader.h"

glTFRenderPassMeshOpaque::glTFRenderPassMeshOpaque()
    : glTFRenderPassMeshBase()
    , m_base_pass_color_render_target(nullptr)
    , m_base_pass_normal_render_target(nullptr)
{
}

bool glTFRenderPassMeshOpaque::ProcessMaterial(glTFRenderResourceManager& resource_manager, const glTFMaterialBase& material)
{
    RETURN_IF_FALSE(material.GetMaterialType() == MaterialType::Opaque)
    // Material texture resource descriptor is alloc within current heap
	RETURN_IF_FALSE(resource_manager.GetMaterialManager().InitMaterialRenderResource(resource_manager, *m_main_descriptor_heap, material))
    
    return true;
}

bool glTFRenderPassMeshOpaque::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
        {m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get()}, &resource_manager.GetDepthRT()))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list,
        {m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get()}))
    
    return true;
}

size_t glTFRenderPassMeshOpaque::GetMainDescriptorHeapSize()
{
    // TODO: Calculate heap size
    return 256;
}

bool glTFRenderPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupRootSignature(resource_manager))
    
    // TODO: Init sampler in material resource manager
    RETURN_IF_FALSE(m_root_signature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear))
    
    return true;
}

bool glTFRenderPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFRenderPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipeline_state_object->BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    IRHIRenderTargetDesc render_target_base_color_desc;
    render_target_base_color_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_base_color_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_base_color_desc.name = "BasePassColor";
    render_target_base_color_desc.isUAV = true;
    render_target_base_color_desc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_base_pass_color_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_base_color_desc);

    IRHIRenderTargetDesc render_target_normal_desc;
    render_target_normal_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_normal_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_normal_desc.name = "BasePassNormal";
    render_target_normal_desc.isUAV = true;
    render_target_normal_desc.clearValue.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    m_base_pass_normal_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_normal_desc);

    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_base_pass_color_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassNormal", m_base_pass_normal_render_target);
    
    m_pipeline_state_object->BindRenderTargets({m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get(), &resource_manager.GetDepthRT()});
    
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
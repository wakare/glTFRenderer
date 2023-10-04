#include "glTFGraphicsPassMeshOpaque.h"

#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFMaterial/glTFMaterialOpaque.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFUtils/glTFImageLoader.h"

glTFGraphicsPassMeshOpaque::glTFGraphicsPassMeshOpaque()
    : m_base_pass_color_render_target(nullptr)
    , m_base_pass_normal_render_target(nullptr)
    , m_material_uploaded(false)
{
    //AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMeshMaterial>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneMaterial>());
    
    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Warp, RHIStaticSamplerFilterMode::Linear>>();
    sampler_interface->SetSamplerRegisterIndexName("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
}

bool glTFGraphicsPassMeshOpaque::InitPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::InitPass(resource_manager))

    return true;
}

bool glTFGraphicsPassMeshOpaque::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().BindRenderTarget(command_list,
        {m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get()}, &resource_manager.GetDepthRT()))

    RETURN_IF_FALSE(resource_manager.GetRenderTargetManager().ClearRenderTarget(command_list,
        {m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get()}))

    if (!m_material_uploaded)
    {
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, *m_main_descriptor_heap);
        m_material_uploaded = true;
    }
    
    return true;
}

size_t glTFGraphicsPassMeshOpaque::GetMainDescriptorHeapSize()
{
    // TODO: Calculate heap size
    return 512;
}

bool glTFGraphicsPassMeshOpaque::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupRootSignature(resource_manager))
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFGraphicsPassMeshBase::SetupPipelineStateObject(resource_manager))
    
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonVS.hlsl)", RHIShaderType::Vertex, "main");
    GetGraphicsPipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\MeshPassCommonPS.hlsl)", RHIShaderType::Pixel, "main");

    IRHIRenderTargetDesc render_target_base_color_desc;
    render_target_base_color_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_base_color_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_base_color_desc.name = "BasePassColor";
    render_target_base_color_desc.isUAV = true;
    render_target_base_color_desc.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB;
    render_target_base_color_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    
    m_base_pass_color_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_base_color_desc);

    IRHIRenderTargetDesc render_target_normal_desc;
    render_target_normal_desc.width = resource_manager.GetSwapchain().GetWidth();
    render_target_normal_desc.height = resource_manager.GetSwapchain().GetHeight();
    render_target_normal_desc.name = "BasePassNormal";
    render_target_normal_desc.isUAV = true;
    render_target_normal_desc.clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB;
    render_target_normal_desc.clearValue.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    m_base_pass_normal_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_normal_desc);

    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_base_pass_color_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassNormal", m_base_pass_normal_render_target);
    
    GetGraphicsPipelineStateObject().BindRenderTargets({m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get(), &resource_manager.GetDepthRT()});
    
    return true;
}

bool glTFGraphicsPassMeshOpaque::BeginDrawMesh(glTFRenderResourceManager& resource_manager, glTFUniqueID meshID)
{
    return true;
    
    // Using texture SRV slot when mesh material is texture
    auto mesh = resource_manager.GetMeshManager().GetMeshes().find(meshID);
    if (mesh == resource_manager.GetMeshManager().GetMeshes().end())
    {
        return false;
    }

    const glTFUniqueID material_ID = mesh->second.material_id;
    if (material_ID == glTFUniqueIDInvalid)
    {
        return true;
    }
    
	return resource_manager.ApplyMaterial(*m_main_descriptor_heap,
	    material_ID, GetRenderInterface<glTFRenderInterfaceSceneMeshMaterial>()->GetRSAllocation().parameter_index, GetPipelineType() == PipelineType::Graphics);
}
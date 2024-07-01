#include "glTFGraphicsPassMeshOpaque.h"

#include "glTFRenderPass/glTFRenderMaterialManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceRadiosityScene.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneMaterial.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "SceneFileLoader/glTFImageLoader.h"

glTFGraphicsPassMeshOpaque::glTFGraphicsPassMeshOpaque()
    : m_base_pass_color_render_target(nullptr)
    , m_base_pass_normal_render_target(nullptr)
    , m_material_uploaded(false)
{
    AddRenderInterface(std::make_shared<glTFRenderInterfaceRadiosityScene>());
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
        GetRenderInterface<glTFRenderInterfaceSceneMaterial>()->UploadMaterialData(resource_manager, MainDescriptorHeapRef());
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

    RHITextureDesc render_target_base_color_desc
    {
        resource_manager.GetSwapChain().GetWidth(),
        resource_manager.GetSwapChain().GetHeight(),
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        true,
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
    "BASEPASS_COLOR_OUTPUT"
    };
    m_base_pass_color_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_base_color_desc);

    RHITextureDesc render_target_normal_desc
    {
        resource_manager.GetSwapChain().GetWidth(),
        resource_manager.GetSwapChain().GetHeight(),
        RHIDataFormat::R8G8B8A8_UNORM_SRGB,
        true,
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        "BASEPASS_NORMAL_OUTPUT"
    };
    m_base_pass_normal_render_target = resource_manager.GetRenderTargetManager().CreateRenderTarget(
        resource_manager.GetDevice(), RHIRenderTargetType::RTV, RHIDataFormat::R8G8B8A8_UNORM_SRGB, RHIDataFormat::R8G8B8A8_UNORM_SRGB, render_target_normal_desc);

    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassColor", m_base_pass_color_render_target);
    resource_manager.GetRenderTargetManager().RegisterRenderTargetWithTag("BasePassNormal", m_base_pass_normal_render_target);
    
    GetGraphicsPipelineStateObject().BindRenderTargetFormats({m_base_pass_color_render_target.get(), m_base_pass_normal_render_target.get(), &resource_manager.GetDepthRT()});
    
    return true;
}
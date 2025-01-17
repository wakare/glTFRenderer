#include "glTFComputePassLighting.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSceneView.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

glTFComputePassLighting::glTFComputePassLighting()
    : m_dispatch_count({0, 0, 0})
    , m_base_color_SRV(nullptr)
    , m_depth_SRV(nullptr)
    , m_normal_SRV(nullptr)
    , m_output_UAV(nullptr)
{
    
}

const char* glTFComputePassLighting::PassName()
{
    return "LightComputePass";
}

bool glTFComputePassLighting::InitRenderInterface(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitRenderInterface(resource_manager))
    
    AddRenderInterface(std::make_shared<glTFRenderInterfaceSceneView>());
    AddRenderInterface(std::make_shared<glTFRenderInterfaceLighting>());

    const std::shared_ptr<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>> sampler_interface =
        std::make_shared<glTFRenderInterfaceSampler<RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear>>("DEFAULT_SAMPLER_REGISTER_INDEX");
    AddRenderInterface(sampler_interface);
    
    return true;
}

bool glTFComputePassLighting::InitPass(glTFRenderResourceManager& resource_manager)
{
    return glTFComputePassBase::InitPass(resource_manager);
}

bool glTFComputePassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::Depth)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_albedo_allocation, *m_base_color_SRV);
    BindDescriptor(command_list, m_depth_allocation, *m_depth_SRV);
    BindDescriptor(command_list, m_normal_allocation, *m_normal_SRV);
    BindDescriptor(command_list, m_output_allocation, *m_output_UAV);
    
    RETURN_IF_FALSE(GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateCPUBuffer(resource_manager))

    return true;
}

bool glTFComputePassLighting::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Copy compute result to swapchain back buffer
    resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    RETURN_IF_FALSE(RHIUtils::Instance().CopyTexture(command_list,
        resource_manager.GetCurrentFrameSwapChainTexture(),
        *GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)))

    return true;
}

DispatchCount glTFComputePassLighting::GetDispatchCount() const
{
    return m_dispatch_count;
} 

bool glTFComputePassLighting::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
                                                    const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }
    
    return GetRenderInterface<glTFRenderInterfaceLighting>()->UpdateLightInfo(*light);
}

bool glTFComputePassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::FinishProcessSceneObject(resource_manager))

    return true;
}

bool glTFComputePassLighting::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))

    AddExportTextureResource(RHITextureDesc::MakeLightingPassOutputTextureDesc(resource_manager), RenderPassResourceTableId::LightingPass_Output);
    AddImportTextureResource(RHITextureDesc::MakeDepthTextureDesc(resource_manager), RenderPassResourceTableId::Depth);
    AddImportTextureResource(RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Albedo);
    AddImportTextureResource(RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager), RenderPassResourceTableId::BasePass_Normal);

    return true;
}

bool glTFComputePassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ALBEDO_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_albedo_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("DEPTH_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("NORMAL_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::SRV, 1, false, m_normal_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_TEX_REGISTER_INDEX", RHIRootParameterDescriptorRangeType::UAV, 1, false, m_output_allocation))

    return true;
}

bool glTFComputePassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))

    m_dispatch_count = {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
    
    auto albedo = GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo);
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), albedo,
                        {albedo->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_base_color_SRV))

    auto depth = GetResourceTexture(RenderPassResourceTableId::Depth);
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), depth,
                        {RHIDataFormat::R32_FLOAT, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_depth_SRV))

    auto normal = GetResourceTexture(RenderPassResourceTableId::BasePass_Normal);
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), normal,
                        {normal->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV}, m_normal_SRV))

    auto lighting_output = GetResourceTexture(RenderPassResourceTableId::LightingPass_Output);
    RETURN_IF_FALSE(resource_manager.GetMemoryManager().GetDescriptorManager().CreateDescriptor(resource_manager.GetDevice(), lighting_output,
                        {lighting_output->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV}, m_output_UAV))

    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\LightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();

    // Add albedo, normal, depth register define
    m_albedo_allocation.AddShaderDefine(shaderMacros);
    m_depth_allocation.AddShaderDefine(shaderMacros);
    m_normal_allocation.AddShaderDefine(shaderMacros);
    m_output_allocation.AddShaderDefine(shaderMacros);
    
    return true;
}
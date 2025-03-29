#include "glTFComputePassLighting.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderSystem/Shadow/ShadowRenderSystem.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "glTFRHI/RHIInterface/IRHISwapChain.h"

struct ShadowMapInfo
{
    inline static std::string Name = "SHADOW_MAP_MATRIX_INFO_REGISTER_INDEX";
    
    glm::mat4 view_matrix{1.0f};
    glm::mat4 projection_matrix{1.0f};
    unsigned shadowmap_width;
    unsigned shadowmap_height;
    unsigned pad[2];
};

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

    AddRenderInterface(std::make_shared<glTFRenderInterfaceStructuredBuffer<ShadowMapInfo>>());
    
    return true;
}

bool glTFComputePassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        GetResourceTexture(RenderPassResourceTableId::ShadowPass_Output)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);    
    }
    
    GetResourceTexture(RenderPassResourceTableId::Depth)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_albedo_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo));
    BindDescriptor(command_list, m_depth_allocation, *GetResourceDescriptor(RenderPassResourceTableId::Depth));
    BindDescriptor(command_list, m_normal_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal));
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        BindDescriptor(command_list, m_shadowmap_allocation, *GetResourceDescriptor(RenderPassResourceTableId::ShadowPass_Output));
    }
    BindDescriptor(command_list, m_output_allocation, *GetResourceDescriptor(RenderPassResourceTableId::LightingPass_Output));

    auto& render_data = resource_manager.GetPerFrameRenderResourceData()[resource_manager.GetCurrentBackBufferIndex()];
    const auto& shadowmap_views = render_data.GetShadowmapSceneViews();
    std::vector<ShadowMapInfo> shadowmap_matrixs; shadowmap_matrixs.reserve(shadowmap_views.size());
    for (auto& shadowmap_view : shadowmap_views)
    {
        ShadowMapInfo info{};
        info.view_matrix = shadowmap_view.second.view_matrix;
        info.projection_matrix = shadowmap_view.second.projection_matrix;
        info.shadowmap_width = ShadowRenderSystem::SHADOWMAP_SIZE;
        info.shadowmap_height = ShadowRenderSystem::SHADOWMAP_SIZE;
        shadowmap_matrixs.push_back(info);
    }

    GetRenderInterface<glTFRenderInterfaceStructuredBuffer<ShadowMapInfo>>()->UploadBuffer(resource_manager, shadowmap_matrixs.data(), 0, shadowmap_matrixs.size() * sizeof(ShadowMapInfo));
    
    return true;
}

DispatchCount glTFComputePassLighting::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    return {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
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

bool glTFComputePassLighting::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))

    auto lighting_output_desc = RHITextureDesc::MakeLightingPassOutputTextureDesc(resource_manager);
    AddExportTextureResource(RenderPassResourceTableId::LightingPass_Output, lighting_output_desc, 
        {lighting_output_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV});
    
    auto depth_desc = RHITextureDesc::MakeDepthTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::Depth, depth_desc,
        {RHIDataFormat::D32_SAMPLE_RESERVED, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto albedo_desc = RHITextureDesc::MakeBasePassAlbedoTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Albedo, albedo_desc,
        {albedo_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto normal_desc = RHITextureDesc::MakeBasePassNormalTextureDesc(resource_manager);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Normal, normal_desc,
        {normal_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});
    
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        auto shadowmap_desc = RHITextureDesc::MakeShadowPassOutputDesc(resource_manager);
        AddImportTextureResource(RenderPassResourceTableId::ShadowPass_Output, shadowmap_desc,
            {  RHIDataFormat::D32_SAMPLE_RESERVED, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV });
    }
    
    AddFinalOutputCandidate(RenderPassResourceTableId::LightingPass_Output);
    
    return true;
}

bool glTFComputePassLighting::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("ALBEDO_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_albedo_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("DEPTH_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_depth_allocation))
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("NORMAL_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_normal_allocation))
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("SHADOWMAP_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::SRV, 1, false, false}, m_shadowmap_allocation))
    }
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_output_allocation))

    return true;
}

bool glTFComputePassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\LightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shaderMacros = GetComputePipelineStateObject().GetShaderMacros();

    // Add albedo, normal, depth register define
    m_albedo_allocation.AddShaderDefine(shaderMacros);
    m_depth_allocation.AddShaderDefine(shaderMacros);
    m_normal_allocation.AddShaderDefine(shaderMacros);
    
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        m_shadowmap_allocation.AddShaderDefine(shaderMacros);
        shaderMacros.AddMacro("HAS_SHADOWMAP", "1");
    }
    m_output_allocation.AddShaderDefine(shaderMacros);
    
    return true;
}
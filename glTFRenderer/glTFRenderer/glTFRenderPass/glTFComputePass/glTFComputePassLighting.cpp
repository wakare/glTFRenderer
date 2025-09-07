#include "glTFComputePassLighting.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceSampler.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceViewBase.h"
#include "glTFRenderPass/glTFRenderInterface/glTFRenderInterfaceVT.h"
#include "glTFRenderSystem/Shadow/ShadowRenderSystem.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "RHIUtils.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHISwapChain.h"

struct ShadowMapInfo
{
    inline static std::string Name = "SHADOW_MAP_MATRIX_INFO_REGISTER_INDEX";
    
    glm::mat4 view_matrix{1.0f};
    glm::mat4 projection_matrix{1.0f};
    unsigned shadowmap_width;
    unsigned shadowmap_height;
    unsigned shadowmap_vsm_texture_id;
    unsigned pad;
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

    if (resource_manager.GetRenderSystem<ShadowRenderSystem>() && resource_manager.GetRenderSystem<ShadowRenderSystem>()->IsVSM())
    {
        AddRenderInterface(std::make_shared<glTFRenderInterfaceVT>(InterfaceVTType::SAMPLE_VT_TEXTURE_DATA));
    }
    
    return true;
}

bool glTFComputePassLighting::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Albedo)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::BasePass_Normal)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);
    GetResourceTexture(RenderPassResourceTableId::Depth)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);

    BindDescriptor(command_list, m_albedo_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Albedo));
    BindDescriptor(command_list, m_depth_allocation, *GetResourceDescriptor(RenderPassResourceTableId::Depth));
    BindDescriptor(command_list, m_normal_allocation, *GetResourceDescriptor(RenderPassResourceTableId::BasePass_Normal));
    BindDescriptor(command_list, m_output_allocation, *GetResourceDescriptor(RenderPassResourceTableId::LightingPass_Output));
    
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>() )
    {
        if (resource_manager.GetRenderSystem<ShadowRenderSystem>()->IsVSM())
        {
        }
        else
        {
            GetResourceTexture(RenderPassResourceTableId::ShadowPass_Output)->Transition(command_list, RHIResourceStateType::STATE_NON_PIXEL_SHADER_RESOURCE);    
            BindDescriptor(command_list, m_shadowmap_allocation, *GetResourceDescriptor(RenderPassResourceTableId::ShadowPass_Output));        
        }
        
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
            if (resource_manager.GetRenderSystem<ShadowRenderSystem>()->IsVSM())
            {
                info.shadowmap_vsm_texture_id = resource_manager.GetRenderSystem<ShadowRenderSystem>()->GetVSM()[0]->GetTextureId();
            }
            shadowmap_matrixs.push_back(info);
        }

        GetRenderInterface<glTFRenderInterfaceStructuredBuffer<ShadowMapInfo>>()->UploadBuffer(resource_manager, shadowmap_matrixs.data(), 0, shadowmap_matrixs.size() * sizeof(ShadowMapInfo));
    }
    
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

    const unsigned width = resource_manager.GetSwapChain().GetWidth();
    const unsigned height = resource_manager.GetSwapChain().GetHeight();
    
    auto lighting_output_desc = RHITextureDesc::MakeLightingPassOutputTextureDesc(width, height);
    AddExportTextureResource(RenderPassResourceTableId::LightingPass_Output, lighting_output_desc, 
        {lighting_output_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV});
    
    auto depth_desc = RHITextureDesc::MakeDepthTextureDesc(width, height);
    AddImportTextureResource(RenderPassResourceTableId::Depth, depth_desc,
        {RHIDataFormat::D32_SAMPLE_RESERVED, RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto albedo_desc = RHITextureDesc::MakeBasePassAlbedoTextureDesc(width, height);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Albedo, albedo_desc,
        {albedo_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});

    auto normal_desc = RHITextureDesc::MakeBasePassNormalTextureDesc(width, height);
    AddImportTextureResource(RenderPassResourceTableId::BasePass_Normal, normal_desc,
        {normal_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_SRV});
    
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>() && !resource_manager.GetRenderSystem<ShadowRenderSystem>()->IsVSM())
    {
        auto shadowmap_desc = RHITextureDesc::MakeShadowPassOutputDesc(width, height);
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
    
    BindShaderCode(
        R"(glTFResources\ShaderSource\ComputeShader\LightingCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetShaderMacros();

    // Add albedo, normal, depth register define
    m_albedo_allocation.AddShaderDefine(shader_macros);
    m_depth_allocation.AddShaderDefine(shader_macros);
    m_normal_allocation.AddShaderDefine(shader_macros);
    
    if (resource_manager.GetRenderSystem<ShadowRenderSystem>())
    {
        m_shadowmap_allocation.AddShaderDefine(shader_macros);
        shader_macros.AddMacro("USE_SHADOWMAP", "1");
        if (resource_manager.GetRenderSystem<ShadowRenderSystem>()->IsVSM())
        {
            shader_macros.AddMacro("USE_VSM", "1");
        }
    }
    m_output_allocation.AddShaderDefine(shader_macros);
    
    return true;
}
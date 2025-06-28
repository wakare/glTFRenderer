#include "glTFComputePassTestFillColor.h"
#include "RHIUtils.h"
#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "RHIInterface/IRHIPipelineStateObject.h"
#include "RHIInterface/IRHIDescriptorManager.h"

const char* glTFComputePassTestFillColor::PassName()
{
    return "ComputePass_TestFillColor";
}

bool glTFComputePassTestFillColor::PreRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PreRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_UNORDERED_ACCESS);
    BindDescriptor(command_list, m_output_allocation, *GetResourceDescriptor(RenderPassResourceTableId::LightingPass_Output));
    
    return true;
}

bool glTFComputePassTestFillColor::PostRenderPass(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::PostRenderPass(resource_manager))

    auto& command_list = resource_manager.GetCommandListForRecord();
    
    // Copy compute result to swapchain back buffer
    resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
    GetResourceTexture(RenderPassResourceTableId::LightingPass_Output)->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
    RETURN_IF_FALSE(RHIUtilInstanceManager::Instance().CopyTexture(command_list,
            resource_manager.GetCurrentFrameSwapChainTexture(),
            *GetResourceTexture(RenderPassResourceTableId::LightingPass_Output), {}))

    return true;
}

bool glTFComputePassTestFillColor::SetupRootSignature(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupRootSignature(resource_manager))
    
    RETURN_IF_FALSE(m_root_signature_helper.AddTableRootParameter("OUTPUT_TEX_REGISTER_INDEX", {RHIDescriptorRangeType::UAV, 1, false, false}, m_output_allocation))
    
    return true;
}

bool glTFComputePassTestFillColor::SetupPipelineStateObject(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::SetupPipelineStateObject(resource_manager))
    
    GetComputePipelineStateObject().BindShaderCode(
        R"(glTFResources\ShaderSource\TestShaders\TestFillColorCS.hlsl)", RHIShaderType::Compute, "main");
    
    auto& shader_macros = GetComputePipelineStateObject().GetShaderMacros();
    m_output_allocation.AddShaderDefine(shader_macros);
    
    return true;
}

bool glTFComputePassTestFillColor::InitResourceTable(glTFRenderResourceManager& resource_manager)
{
    RETURN_IF_FALSE(glTFComputePassBase::InitResourceTable(resource_manager))

    const unsigned width = resource_manager.GetSwapChain().GetWidth();
    const unsigned height = resource_manager.GetSwapChain().GetHeight();
    
    RHITextureDesc output_texture_desc = RHITextureDesc::MakeFullScreenTextureDesc(
        "ComputePassTestOutputTexture",
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_UAV | RUF_TRANSFER_SRC),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color {0.0f, 0.0f, 0.0f, 0.0f}
        },
        width, height
    );
    AddExportTextureResource(RenderPassResourceTableId::LightingPass_Output, output_texture_desc, 
    {output_texture_desc.GetDataFormat(), RHIResourceDimension::TEXTURE2D, RHIViewType::RVT_UAV});

    return true;
}

DispatchCount glTFComputePassTestFillColor::GetDispatchCount(glTFRenderResourceManager& resource_manager) const
{
    return {resource_manager.GetSwapChain().GetWidth() / 8, resource_manager.GetSwapChain().GetHeight() / 8, 1};
}

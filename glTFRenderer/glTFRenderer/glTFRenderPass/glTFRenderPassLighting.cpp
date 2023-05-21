#include "glTFRenderPassLighting.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIUtils.h"
#include "glTFRenderResourceManager.h"

glTFRenderPassLighting::glTFRenderPassLighting()
    : m_basePassColorRT(nullptr)
    , m_basePassColorRTSRVHandle(0)
{
}

const char* glTFRenderPassLighting::PassName()
{
    return "LightPass";
}

bool glTFRenderPassLighting::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::RenderPass(resourceManager))

    if (m_basePassColorRTSRVHandle)
    {
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE);
        
        RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), 0, m_basePassColorRTSRVHandle);    
    }
    
    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
            &resourceManager.GetCurrentFrameSwapchainRT(), 1, nullptr))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), &resourceManager.GetCurrentFrameSwapchainRT(), 1))
    
    DrawPostprocessQuad(resourceManager);
    
    if (m_basePassColorRTSRVHandle)
    {
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
                RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET);    
    }
    
    return true;
}

bool glTFRenderPassLighting::TryProcessSceneObject(glTFRenderResourceManager& resourceManager,
    const glTFSceneObjectBase& object)
{
    return false;
}

size_t glTFRenderPassLighting::GetMainDescriptorHeapSize()
{
    return 1;
}

bool glTFRenderPassLighting::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupRootSignature(resourceManager))
    
    // Init root signature
    constexpr size_t rootSignatureParameterCount = 1;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))

    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 1};
    m_rootSignature->GetRootParameter(0).InitAsDescriptorTableRange(1, &SRVRangeDesc);
    
    m_rootSignature->GetStaticSampler(0).InitStaticSampler(0, RHIStaticSamplerAddressMode::Clamp, RHIStaticSamplerFilterMode::Linear);
    RETURN_IF_FALSE(m_rootSignature->InitRootSignature(resourceManager.GetDevice()))
    
    return true;
}

bool glTFRenderPassLighting::SetupPipelineStateObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupPipelineStateObject(resourceManager))

    std::vector<IRHIRenderTarget*> allRts;
    allRts.push_back(&resourceManager.GetCurrentFrameSwapchainRT());
    m_pipelineStateObject->BindRenderTargets(allRts);
    
    m_basePassColorRT = resourceManager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassColor");
    if (m_basePassColorRT)
    {
        RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 0,
            *m_basePassColorRT, {m_basePassColorRT->GetRenderTargetFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_basePassColorRTSRVHandle))    
    }
    
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE (m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain(), GetVertexInputLayout()))
    
    return true;
}

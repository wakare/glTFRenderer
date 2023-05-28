#include "glTFRenderPassLighting.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIUtils.h"
#include "glTFRenderResourceManager.h"
#include "../glTFRHI/RHIResourceFactory.h"

glTFRenderPassLighting::glTFRenderPassLighting()
    : glTFRenderPassInterfaceSceneView(LightPass_RootParameter_SceneView, LightPass_RootParameter_SceneView)
    , m_basePassColorRT(nullptr)
    , m_basePassColorRTSRVHandle(0)
    , m_depthRTSRVHandle(0)
    , m_constantBufferPerLightDraw({})
{
    m_constantBufferPerLightDraw.lightInfos[0] = {0.0f, 0.0f, 0.0f, 4.0f};
    m_constantBufferPerLightDraw.lightInfos[1] = {8.0f, 0.0f, 0.0f, 3.0f};
    m_constantBufferPerLightDraw.lightInfos[2] = {-8.0f, 0.0f, 0.0f, 2.0f};
    m_constantBufferPerLightDraw.lightInfos[3] = {0.0f, 0.0f, 1.0f, 4.0f};
}

const char* glTFRenderPassLighting::PassName()
{
    return "LightPass";
}

bool glTFRenderPassLighting::InitPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::InitPass(resourceManager))
    
    RETURN_IF_FALSE(RHIUtils::Instance().ResetCommandList(resourceManager.GetCommandList(), resourceManager.GetCurrentFrameCommandAllocator()))
    
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::InitInterface(resourceManager))

    m_constantBufferInGPU = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_constantBufferInGPU->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_PerMeshConstantBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }))

    RETURN_IF_FALSE(RHIUtils::Instance().CloseCommandList(resourceManager.GetCommandList()))
    RETURN_IF_FALSE(RHIUtils::Instance().ExecuteCommandList(resourceManager.GetCommandList(),resourceManager.GetCommandQueue()))
    RETURN_IF_FALSE(resourceManager.GetCurrentFrameFence().SignalWhenCommandQueueFinish(resourceManager.GetCommandQueue()))

    return true;
}

bool glTFRenderPassLighting::RenderPass(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassPostprocess::RenderPass(resourceManager))

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resourceManager, LightPass_RootParameter_SceneView))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetDepthRT(),
        RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_BaseColorAndDepthSRV, m_mainDescriptorHeap->GetHandle(0)))    

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
        &resourceManager.GetCurrentFrameSwapchainRT(), 1, nullptr))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), &resourceManager.GetCurrentFrameSwapchainRT(), 1))

    RETURN_IF_FALSE(m_constantBufferInGPU->UploadBufferFromCPU(&m_constantBufferPerLightDraw, 0, sizeof(m_constantBufferPerLightDraw)))
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_LightInfos, m_constantBufferInGPU->GetGPUBufferHandle()))
    
    DrawPostprocessQuad(resourceManager);
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetDepthRT(),
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::DEPTH_WRITE))
    
    return true;
}

bool glTFRenderPassLighting::TryProcessSceneObject(glTFRenderResourceManager& resourceManager,
    const glTFSceneObjectBase& object)
{
    return false;
}

size_t glTFRenderPassLighting::GetMainDescriptorHeapSize()
{
    return 2;
}

bool glTFRenderPassLighting::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = LightPass_RootParameter_Num;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))

    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupRootSignature(resourceManager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_rootSignature))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 2};
    m_rootSignature->GetRootParameter(LightPass_RootParameter_BaseColorAndDepthSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);

    m_rootSignature->GetRootParameter(LightPass_RootParameter_LightInfos).InitAsCBV(2);
    
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
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 0,
            *m_basePassColorRT, {m_basePassColorRT->GetRenderTargetFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_basePassColorRTSRVHandle))

    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 1,
            resourceManager.GetDepthRT(), {RHIDataFormat::R32_FLOAT, RHIShaderVisibleViewDimension::TEXTURE2D}, m_depthRTSRVHandle))
    
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassVS.hlsl)", RHIShaderType::Vertex, "main");
    m_pipelineStateObject->BindShaderCode(
        R"(glTFResources\ShaderSource\LightPassPS.hlsl)", RHIShaderType::Pixel, "main");

    RETURN_IF_FALSE(m_pipelineStateObject->BindInputLayout(GetVertexInputLayout()))
    
    auto& shaderMacros = m_pipelineStateObject->GetShaderMacros();
    glTFRenderPassInterfaceSceneView::UpdateShaderCompileDefine(shaderMacros);
    
    RETURN_IF_FALSE (m_pipelineStateObject->InitPipelineStateObject(resourceManager.GetDevice(), *m_rootSignature, resourceManager.GetSwapchain()))
    
    return true;
}

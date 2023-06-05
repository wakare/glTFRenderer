#include "glTFRenderPassLighting.h"
#include "../glTFRHI/RHIInterface/IRHIPipelineStateObject.h"
#include "../glTFRHI/RHIInterface/IRHIRenderTargetManager.h"
#include "../glTFRHI/RHIUtils.h"
#include "glTFRenderResourceManager.h"
#include "../glTFLight/glTFDirectionalLight.h"
#include "../glTFLight/glTFLightBase.h"
#include "../glTFLight/glTFPointLight.h"
#include "../glTFRHI/RHIResourceFactory.h"

glTFRenderPassLighting::glTFRenderPassLighting()
    : glTFRenderPassInterfaceSceneView(LightPass_RootParameter_SceneViewCBV, LightPass_RootParameter_SceneViewCBV)
    , m_basePassColorRT(nullptr)
    , m_normalRT(nullptr)
    , m_basePassColorRTSRVHandle(0)
    , m_depthRTSRVHandle(0)
    , m_normalRTSRVHandle(0)
    , m_constantBufferPerLightDraw({})
{
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

    m_lightInfoGPUConstantBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_pointLightInfoGPUStructuredBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    m_directionalLightInfoGPUStructuredBuffer = RHIResourceFactory::CreateRHIResource<IRHIGPUBuffer>();
    
    // TODO: Calculate mesh constant buffer size
    RETURN_IF_FALSE(m_lightInfoGPUConstantBuffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_LightInfoConstantBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::Unknown,
            RHIBufferResourceType::Buffer
        }))

    RETURN_IF_FALSE(m_pointLightInfoGPUStructuredBuffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_PointLightInfoStructuredBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::R32G32B32A32_FLOAT,
            RHIBufferResourceType::Buffer
        }))

    RETURN_IF_FALSE(m_directionalLightInfoGPUStructuredBuffer->InitGPUBuffer(resourceManager.GetDevice(),
        {
            L"LightPass_DirectionalLightInfoStructuredBuffer",
            static_cast<size_t>(64 * 1024),
            1,
            1,
            RHIBufferType::Upload,
            RHIDataFormat::R32G32B32A32_FLOAT,
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

    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::ApplyInterface(resourceManager, LightPass_RootParameter_SceneViewCBV))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_normalRT,
        RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PIXEL_SHADER_RESOURCE))
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetDepthRT(),
        RHIResourceStateType::DEPTH_WRITE, RHIResourceStateType::PIXEL_SHADER_RESOURCE))

    RETURN_IF_FALSE(RHIUtils::Instance().SetDescriptorTableGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_BaseColorAndDepthSRV, m_mainDescriptorHeap->GetHandle(0)))    

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().BindRenderTarget(resourceManager.GetCommandList(),
        {&resourceManager.GetCurrentFrameSwapchainRT()}, nullptr))

    RETURN_IF_FALSE(resourceManager.GetRenderTargetManager().ClearRenderTarget(resourceManager.GetCommandList(), {&resourceManager.GetCurrentFrameSwapchainRT()}))

    RETURN_IF_FALSE(UploadLightInfoGPUBuffer())
    RETURN_IF_FALSE(RHIUtils::Instance().SetConstantBufferViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_LightInfosCBV, m_lightInfoGPUConstantBuffer->GetGPUBufferHandle()))
    RHIUtils::Instance().SetShaderResourceViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_PointLightStructuredBuffer, m_pointLightInfoGPUStructuredBuffer->GetGPUBufferHandle());
    RHIUtils::Instance().SetShaderResourceViewGPUHandleToRootParameterSlot(resourceManager.GetCommandList(), LightPass_RootParameter_DirectionalLightStructuredBuffer, m_directionalLightInfoGPUStructuredBuffer->GetGPUBufferHandle());
    DrawPostprocessQuad(resourceManager);
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_basePassColorRT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), *m_normalRT,
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::RENDER_TARGET))

    RETURN_IF_FALSE(RHIUtils::Instance().AddRenderTargetBarrierToCommandList(resourceManager.GetCommandList(), resourceManager.GetDepthRT(),
        RHIResourceStateType::PIXEL_SHADER_RESOURCE, RHIResourceStateType::DEPTH_WRITE))
    
    return true;
}

bool glTFRenderPassLighting::TryProcessSceneObject(glTFRenderResourceManager& resourceManager,
    const glTFSceneObjectBase& object)
{
    const glTFLightBase* light = dynamic_cast<const glTFLightBase*>(&object);
    if (!light)
    {
        return false;
    }

    switch (light->GetType())
    {
    case glTFLightType::PointLight:
        {
            const glTFPointLight* pointLight = dynamic_cast<const glTFPointLight*>(light);
            PointLightInfo pointLightInfo{};
            pointLightInfo.positionAndRadius = glm::vec4(pointLight->GetTransform().position, pointLight->GetRadius());
            pointLightInfo.intensityAndFalloff = {pointLight->GetIntensity(), pointLight->GetFalloff(), 0.0f, 0.0f};
            m_cachePointLights[pointLight->GetID()] = pointLightInfo;
        }
        break;
        
    case glTFLightType::DirectionalLight:
        {
            const glTFDirectionalLight* directionalLight = dynamic_cast<const glTFDirectionalLight*>(light);
            DirectionalLightInfo directionalLightInfo{};
            directionalLightInfo.directionalAndIntensity = glm::vec4(directionalLight->GetDirection(), directionalLight->GetIntensity());
            m_cacheDirectionalLights[directionalLight->GetID()] = directionalLightInfo;
        }
        break;
    case glTFLightType::SpotLight: break;
    case glTFLightType::SkyLight: break;
    default: ;
    }
    
    return true;
}

bool glTFRenderPassLighting::FinishProcessSceneObject(glTFRenderResourceManager& resourceManager)
{
    RETURN_IF_FALSE(glTFRenderPassBase::FinishProcessSceneObject(resourceManager))

    // Update light info gpu buffer
    m_constantBufferPerLightDraw.pointLightCount = m_cachePointLights.size();
    m_constantBufferPerLightDraw.directionalLightCount = m_cacheDirectionalLights.size();
    
    m_constantBufferPerLightDraw.pointLightInfos.resize(m_constantBufferPerLightDraw.pointLightCount);
    m_constantBufferPerLightDraw.directionalInfos.resize(m_constantBufferPerLightDraw.directionalLightCount);

    size_t pointLightIndex = 0;
    for (const auto& pointLight : m_cachePointLights)
    {
        m_constantBufferPerLightDraw.pointLightInfos[pointLightIndex++] = pointLight.second;
    }

    size_t directionalLightIndex = 0;
    for (const auto& directionalLight : m_cacheDirectionalLights)
    {
        m_constantBufferPerLightDraw.directionalInfos[directionalLightIndex++] = directionalLight.second;
    }
    
    return true;
}

size_t glTFRenderPassLighting::GetMainDescriptorHeapSize()
{
    return 3;
}

bool glTFRenderPassLighting::SetupRootSignature(glTFRenderResourceManager& resourceManager)
{
    // Init root signature
    constexpr size_t rootSignatureParameterCount = LightPass_RootParameter_Num;
    constexpr size_t rootSignatureStaticSamplerCount = 1;
    RETURN_IF_FALSE(m_rootSignature->AllocateRootSignatureSpace(rootSignatureParameterCount, rootSignatureStaticSamplerCount))

    RETURN_IF_FALSE(glTFRenderPassPostprocess::SetupRootSignature(resourceManager))
    RETURN_IF_FALSE(glTFRenderPassInterfaceSceneView::SetupRootSignature(*m_rootSignature))
    
    const RHIRootParameterDescriptorRangeDesc SRVRangeDesc {RHIRootParameterDescriptorRangeType::SRV, 0, 3};
    m_rootSignature->GetRootParameter(LightPass_RootParameter_BaseColorAndDepthSRV).InitAsDescriptorTableRange(1, &SRVRangeDesc);

    m_rootSignature->GetRootParameter(LightPass_RootParameter_LightInfosCBV).InitAsCBV(1);
    m_rootSignature->GetRootParameter(LightPass_RootParameter_PointLightStructuredBuffer).InitAsSRV(3);
    m_rootSignature->GetRootParameter(LightPass_RootParameter_DirectionalLightStructuredBuffer).InitAsSRV(4);
    
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

    m_normalRT = resourceManager.GetRenderTargetManager().GetRenderTargetWithTag("BasePassNormal");
    RETURN_IF_FALSE(RHIUtils::Instance().CreateShaderResourceViewInDescriptorHeap(resourceManager.GetDevice(), *m_mainDescriptorHeap, 2,
            *m_normalRT, {m_normalRT->GetRenderTargetFormat(), RHIShaderVisibleViewDimension::TEXTURE2D}, m_normalRTSRVHandle))
    
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

bool glTFRenderPassLighting::UploadLightInfoGPUBuffer()
{
    // First upload size of light infos
    RETURN_IF_FALSE(m_lightInfoGPUConstantBuffer->UploadBufferFromCPU(&m_constantBufferPerLightDraw, 0,
        sizeof(m_constantBufferPerLightDraw.pointLightCount) + sizeof(m_constantBufferPerLightDraw.directionalLightCount)))

    if (!m_constantBufferPerLightDraw.pointLightInfos.empty())
    {
        RETURN_IF_FALSE(m_pointLightInfoGPUStructuredBuffer->UploadBufferFromCPU(m_constantBufferPerLightDraw.pointLightInfos.data(),
            0, m_constantBufferPerLightDraw.pointLightInfos.size() * sizeof(m_constantBufferPerLightDraw.pointLightInfos[0])))
    }

    if (!m_constantBufferPerLightDraw.directionalInfos.empty())
    {
        RETURN_IF_FALSE(m_directionalLightInfoGPUStructuredBuffer->UploadBufferFromCPU(m_constantBufferPerLightDraw.directionalInfos.data(),
            0, m_constantBufferPerLightDraw.directionalInfos.size() * sizeof(m_constantBufferPerLightDraw.directionalInfos[0])))
    }
    
    return true;
}

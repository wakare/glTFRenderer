#include "glTFRenderPassManager.h"

#include <cassert>
#include <Windows.h>

#include "glTFRenderPassMeshBase.h"
#include "../glTFLoader/glTFElementCommon.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassManager::glTFRenderPassManager(glTFWindow& window, glTFSceneView& view)
    : m_window(window)
    , m_sceneView(view)
    , m_frameIndex(0)
{
    const bool inited = InitRenderPassManager();
    assert(inited);
}

bool glTFRenderPassManager::InitRenderPassManager()
{
    m_resourceManager.reset(new glTFRenderResourceManager());
    if (!m_resourceManager->InitResourceManager(m_window))
    {
        return false;
    }
    
    return true;
}

void glTFRenderPassManager::AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass)
{
    m_passes.push_back(std::move(pass));
}

void glTFRenderPassManager::InitAllPass()
{
    for (const auto& pass : m_passes)
    {
        const bool inited = pass->InitPass(*m_resourceManager);
        assert(inited);
    }
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(size_t deltaTimeMs)
{
    // Gather all scene pass
    for (const auto& pass : m_passes)
    {
        m_sceneView.TraverseSceneObjectWithinView([this, &pass](const glTFSceneNode& node)
        {
            if (node.renderStateDirty)
            {
                for (const auto& sceneObject : node.m_objects)
                {
                    if (pass->TryProcessSceneObject(*m_resourceManager, *sceneObject))
                    {
                        node.renderStateDirty = false;
                    }    
                }
            }
            
            return true;
        });
    }

    for (const auto& pass : m_passes)
    {
        const bool success = pass->FinishProcessSceneObject(*m_resourceManager);
        GLTF_CHECK(success);

        if (auto* sceneViewInterface = dynamic_cast<glTFRenderPassInterfaceSceneView*>(pass.get()))
        {
            void(m_sceneView.GetViewProjectionMatrix());
            sceneViewInterface->UpdateSceneViewData({
                m_sceneView.GetViewMatrix(),
                m_sceneView.GetProjectionMatrix(),
                inverse(m_sceneView.GetViewMatrix()),
                inverse(m_sceneView.GetProjectionMatrix())});    
        }
    }
}

void glTFRenderPassManager::RenderAllPass(size_t deltaTimeMs)
{
    static auto now = GetTickCount64();
    static unsigned frameCountInOneSecond = 0;
    frameCountInOneSecond++;
    if (GetTickCount64() - now > 1000)
    {
        LOG_FORMAT_FLUSH("[DEBUG] FPS: %d\n", frameCountInOneSecond)
        frameCountInOneSecond = 0;
        now = GetTickCount64();
    }

    // Wait util last frame executed
    m_resourceManager->GetCurrentFrameFence().WaitUtilSignal();
    
    // Reset command allocator when previous frame executed finish...
    RHIUtils::Instance().ResetCommandAllocator(m_resourceManager->GetCurrentFrameCommandAllocator());

    ExecuteCommandSection(nullptr, [this]()
    {
        // Transition swapchain state to render target for shading 
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandList(), m_resourceManager->GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::PRESENT, RHIResourceStateType::RENDER_TARGET);
    });
    
    for (const auto& pass : m_passes)
    {
        ExecuteCommandSection(&pass->GetPSO(), [this, passPtr = pass.get()]()
        {
            const RHIViewportDesc viewport = {0, 0, (float)m_resourceManager->GetSwapchain().GetWidth(), (float)m_resourceManager->GetSwapchain().GetHeight(), 0.0f, 1.0f };
            RHIUtils::Instance().SetViewport(m_resourceManager->GetCommandList(), viewport);

            const RHIScissorRectDesc scissorRect = {0, 0, m_resourceManager->GetSwapchain().GetWidth(), m_resourceManager->GetSwapchain().GetHeight() }; 
            RHIUtils::Instance().SetScissorRect(m_resourceManager->GetCommandList(), scissorRect);
            
            passPtr->RenderPass(*m_resourceManager);
        });
    }
    
    ExecuteCommandSection(nullptr, [this](){
        RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandList(), m_resourceManager->GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);});
    
    m_resourceManager->GetCurrentFrameFence().SignalWhenCommandQueueFinish(m_resourceManager->GetCommandQueue());
    RHIUtils::Instance().Present(m_resourceManager->GetSwapchain());
    
    m_resourceManager->UpdateCurrentBackBufferIndex();
}

void glTFRenderPassManager::ExitAllPass()
{
    m_resourceManager->GetCurrentFrameFence().WaitUtilSignal();
    // Reset command allocator when previous frame executed finish...
    RHIUtils::Instance().ResetCommandAllocator(m_resourceManager->GetCurrentFrameCommandAllocator());
    
    m_passes.clear();
}

void glTFRenderPassManager::ExecuteCommandSection(IRHIPipelineStateObject* PSO, std::function<void()> executeLambda)
{
    if (PSO)
    {
        RHIUtils::Instance().ResetCommandList(m_resourceManager->GetCommandList(),
               m_resourceManager->GetCurrentFrameCommandAllocator(), *PSO);
    }
    else
    {
        RHIUtils::Instance().ResetCommandList(m_resourceManager->GetCommandList(),
               m_resourceManager->GetCurrentFrameCommandAllocator());
    }

    executeLambda();

    RHIUtils::Instance().CloseCommandList(m_resourceManager->GetCommandList());
    
    RHIUtils::Instance().ExecuteCommandList(m_resourceManager->GetCommandList(), m_resourceManager->GetCommandQueue());
    m_resourceManager->GetCurrentFrameFence().SignalWhenCommandQueueFinish(m_resourceManager->GetCommandQueue());
}

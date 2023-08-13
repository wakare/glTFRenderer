#include "glTFRenderPassManager.h"

#include <cassert>
#include <Windows.h>

#include "glTFRenderPassLighting.h"
#include "glTFRenderPassMeshBase.h"
#include "glTFRenderPassMeshOpaque.h"
#include "../glTFLoader/glTFElementCommon.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassManager::glTFRenderPassManager(glTFWindow& window, glTFSceneView& view)
    : m_window(window)
    , m_scene_view(view)
    , m_frame_index(0)
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
	GLTF_CHECK(m_scene_view.SetupRenderPass(*this));
    
    for (const auto& pass : m_passes)
    {
        const bool inited = pass->InitPass(*m_resourceManager);
        assert(inited);
        LOG_FORMAT("[DEBUG] Init pass %s finished!\n", pass->PassName())
    }
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(size_t deltaTimeMs)
{
    // Gather all scene pass
    for (const auto& pass : m_passes)
    {
        m_scene_view.TraverseSceneObjectWithinView([this, &pass](const glTFSceneNode& node)
        {
            if (node.IsDirty())
            {
                for (const auto& sceneObject : node.m_objects)
                {
                    if (pass->TryProcessSceneObject(*m_resourceManager, *sceneObject))
                    {
                        sceneObject->ResetDirty();
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
            sceneViewInterface->UpdateSceneViewData({
                m_scene_view.GetViewMatrix(),
                m_scene_view.GetProjectionMatrix(),
                inverse(m_scene_view.GetViewMatrix()),
                inverse(m_scene_view.GetProjectionMatrix())});    
        }
    }
}

void glTFRenderPassManager::RenderAllPass(size_t deltaTimeMs) const
{
    m_resourceManager->WaitLastFrameFinish();
    
    // Reset command allocator when previous frame executed finish...
    m_resourceManager->ResetCommandAllocator();
    
    static auto now = GetTickCount64();
    static unsigned frameCountInOneSecond = 0;
    frameCountInOneSecond++;
    if (GetTickCount64() - now > 1000)
    {
        LOG_FORMAT_FLUSH("[DEBUG] FPS: %d\n", frameCountInOneSecond)
        frameCountInOneSecond = 0;
        now = GetTickCount64();
    }

    // Transition swapchain state to render target for shading 
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandListForRecord(), m_resourceManager->GetCurrentFrameSwapchainRT(),
    RHIResourceStateType::PRESENT, RHIResourceStateType::RENDER_TARGET);
    
    for (const auto& pass : m_passes)
    {
        pass->RenderPass(*m_resourceManager);
    }
    
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandListForRecord(), m_resourceManager->GetCurrentFrameSwapchainRT(),
            RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);

    m_resourceManager->CloseCommandListAndExecute(false);
    
    RHIUtils::Instance().Present(m_resourceManager->GetSwapchain());
    
    m_resourceManager->UpdateCurrentBackBufferIndex();
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}
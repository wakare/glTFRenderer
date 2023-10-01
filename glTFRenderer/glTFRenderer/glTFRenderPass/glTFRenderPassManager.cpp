#include "glTFRenderPassManager.h"

#include <cassert>
#include <Windows.h>

#include "glTFGraphicsPassLighting.h"
#include "glTFGraphicsPassMeshBase.h"
#include "../glTFLoader/glTFElementCommon.h"
#include "../glTFRHI/RHIUtils.h"
#include "../glTFUtils/glTFLog.h"

glTFRenderPassManager::glTFRenderPassManager(glTFWindow& window, glTFSceneView& view)
    : m_window(window)
    , m_scene_view(view)
    , m_frame_index(0)
{
    const bool inited = InitRenderPassManager();
    GLTF_CHECK(inited);
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

    m_resourceManager->CloseCommandListAndExecute(true);
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(size_t deltaTimeMs)
{
    // Gather all scene pass
    std::vector<glTFRenderPassBase*> graphics_passes;
    graphics_passes.reserve(m_passes.size());
    for (const auto& pass : m_passes)
    {
        graphics_passes.push_back(pass.get());
    }
    
    for (const auto& pass : graphics_passes)
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

    for (const auto& pass : graphics_passes)
    {
        const bool success = pass->FinishProcessSceneObject(*m_resourceManager);
        GLTF_CHECK(success);

        if (auto* sceneViewInterface = dynamic_cast<glTFRenderInterfaceSceneView*>(pass))
        {
            unsigned width, height;
            m_scene_view.GetViewportSize(width, height);
            ConstantBufferSceneView temp_view_data =
            {
                m_scene_view.GetViewMatrix(),
                m_scene_view.GetProjectionMatrix(),
                inverse(m_scene_view.GetViewMatrix()),
                inverse(m_scene_view.GetProjectionMatrix()),
                width, height
            };
            
            sceneViewInterface->UpdateCPUBuffer(&temp_view_data, sizeof(temp_view_data));    
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
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandListForRecord(), m_resourceManager->GetCurrentFrameSwapchainRT(), RHIResourceStateType::PRESENT, RHIResourceStateType::RENDER_TARGET);
    
    for (const auto& pass : m_passes)
    {
        pass->PreRenderPass(*m_resourceManager);
        pass->RenderPass(*m_resourceManager);
        pass->PostRenderPass(*m_resourceManager);
    }
    
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resourceManager->GetCommandListForRecord(), m_resourceManager->GetCurrentFrameSwapchainRT(), RHIResourceStateType::RENDER_TARGET, RHIResourceStateType::PRESENT);

    // TODO: no waiting causing race with base color and normal?
    m_resourceManager->CloseCommandListAndExecute(true);
    
    RHIUtils::Instance().Present(m_resourceManager->GetSwapchain());
    
    m_resourceManager->UpdateCurrentBackBufferIndex();
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}
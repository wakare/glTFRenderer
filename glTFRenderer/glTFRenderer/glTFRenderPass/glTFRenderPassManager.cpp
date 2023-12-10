#include "glTFRenderPassManager.h"

#include <cassert>
#include <Windows.h>

#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshBase.h"
#include "glTFLoader/glTFElementCommon.h"
#include "glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFUtils/glTFLog.h"

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
    m_resource_manager.reset(new glTFRenderResourceManager());
    if (!m_resource_manager->InitResourceManager(m_window))
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
    m_scene_view.TraverseSceneObjectWithinView([&](const glTFSceneNode& node)
    {
        for (const auto& scene_object : node.m_objects)
        {
            m_resource_manager->TryProcessSceneObject(*m_resource_manager, *scene_object);    
        }
        
        return true;
    });

    GLTF_CHECK(m_resource_manager->GetMeshManager().BuildMeshRenderResource(*m_resource_manager));
    
	GLTF_CHECK(m_scene_view.SetupRenderPass(*this));
    
    for (const auto& pass : m_passes)
    {
        const bool inited = pass->InitPass(*m_resource_manager);
        assert(inited);
        LOG_FORMAT("[DEBUG] Init pass %s finished!\n", pass->PassName())
    }

    m_resource_manager->CloseCommandListAndExecute(true);
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(size_t deltaTimeMs)
{
    // Gather all scene pass
    std::vector<glTFRenderPassBase*> render_passes;
    render_passes.reserve(m_passes.size());
    for (const auto& pass : m_passes)
    {
        render_passes.push_back(pass.get());
    }

    std::vector<const glTFSceneNode*> dirty_objects;
    m_scene_view.TraverseSceneObjectWithinView([this, &dirty_objects](const glTFSceneNode& node)
    {
        if (node.IsDirty())
        {
            dirty_objects.push_back(&node);
        }

        return true;
    });

    for (const auto* node : dirty_objects)
    {
        for (const auto& scene_object : node->m_objects)
        {
            m_resource_manager->TryProcessSceneObject(*m_resource_manager, *scene_object);
            for (const auto& pass : render_passes)
            {
                pass->TryProcessSceneObject(*m_resource_manager, *scene_object);
            }
        }

        node->ResetDirty();
    }

    for (const auto& pass : render_passes)
    {
        pass->UpdateRenderFlags(m_scene_view.GetRenderFlags());
        
        const bool success = pass->FinishProcessSceneObject(*m_resource_manager);
        GLTF_CHECK(success);

        if (auto* sceneViewInterface = pass->GetRenderInterface<glTFRenderInterfaceSceneView>())
        {
            sceneViewInterface->UpdateSceneView(m_scene_view);
        }

        if (auto* frame_stat = pass->GetRenderInterface<glTFRenderInterfaceFrameStat>())
        {
            //unsigned current_frame = m_resource_manager->GetCurrentBackBufferIndex();
            static unsigned _frame_count = 0;
            ++_frame_count;
            frame_stat->UploadCPUBuffer(&_frame_count, 0, sizeof(_frame_count));
        }
    }
}

void glTFRenderPassManager::RenderAllPass(size_t deltaTimeMs) const
{
    m_resource_manager->WaitLastFrameFinish();
    
    // Reset command allocator when previous frame executed finish...
    m_resource_manager->ResetCommandAllocator();
    
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
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resource_manager->GetCommandListForRecord(), m_resource_manager->GetCurrentFrameSwapchainRT(), RHIResourceStateType::STATE_PRESENT, RHIResourceStateType::STATE_RENDER_TARGET);
    
    for (const auto& pass : m_passes)
    {
        pass->PreRenderPass(*m_resource_manager);
        pass->RenderPass(*m_resource_manager);
        pass->PostRenderPass(*m_resource_manager);
    }
    
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(m_resource_manager->GetCommandListForRecord(), m_resource_manager->GetCurrentFrameSwapchainRT(), RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PRESENT);

    // TODO: no waiting causing race with base color and normal?
    m_resource_manager->CloseCommandListAndExecute(true);
    
    RHIUtils::Instance().Present(m_resource_manager->GetSwapchain());
    
    m_resource_manager->UpdateCurrentBackBufferIndex();
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}
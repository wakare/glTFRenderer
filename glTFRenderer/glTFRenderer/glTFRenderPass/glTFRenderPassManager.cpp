#include "glTFRenderPassManager.h"

#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshBase.h"
#include "glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRHI/RHIUtils.h"
#include "glTFUtils/glTFLog.h"
#include "glTFWindow/glTFWindow.h"

glTFRenderPassManager::glTFRenderPassManager(const glTFSceneView& view)
    : m_scene_view(view)
    , m_frame_index(0)
{
}
 
bool glTFRenderPassManager::InitRenderPassManager(const glTFWindow& window)
{
    return true;
}

void glTFRenderPassManager::AddRenderPass(std::unique_ptr<glTFRenderPassBase>&& pass)
{
    m_passes.push_back(std::move(pass));
}

void glTFRenderPassManager::InitAllPass(glTFRenderResourceManager& resource_manager)
{
    m_scene_view.TraverseSceneObjectWithinView([&](const glTFSceneNode& node)
    {
        for (const auto& scene_object : node.m_objects)
        {
            resource_manager.TryProcessSceneObject(resource_manager, *scene_object);    
        }
        
        return true;
    });

    GLTF_CHECK(resource_manager.GetMeshManager().BuildMeshRenderResource(resource_manager));
    
    for (const auto& pass : m_passes)
    {
        const bool inited = pass->InitPass(resource_manager);
        GLTF_CHECK(inited);
        LOG_FORMAT("[DEBUG] Init pass %s finished!\n", pass->PassName())
    }

    resource_manager.CloseCommandListAndExecute(true);
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs)
{
    // Gather all scene pass
    
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
            resource_manager.TryProcessSceneObject(resource_manager, *scene_object);
            for (const auto& pass : m_passes)
            {
                pass->TryProcessSceneObject(resource_manager, *scene_object);
            }
        }

        node->ResetDirty();
    }

    for (const auto& pass : m_passes)
    {
        const bool success = pass->FinishProcessSceneObject(resource_manager);
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

void glTFRenderPassManager::UpdateAllPassGUIWidgets()
{
    for (const auto& pass : m_passes)
    {
        pass->UpdateGUIWidgets();
    }
}

void glTFRenderPassManager::RenderBegin(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs)
{
    resource_manager.WaitLastFrameFinish();
    
    // Reset command allocator when previous frame executed finish...
    resource_manager.ResetCommandAllocator();

    // Transition swapchain state to render target for shading 
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(
        resource_manager.GetCommandListForRecord(),
        resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_PRESENT, RHIResourceStateType::STATE_RENDER_TARGET);
}

void glTFRenderPassManager::UpdatePipelineOptions(const glTFPassOptionRenderFlags& pipeline_options)
{
    for (const auto& pass : m_passes)
    {
        pass->UpdateRenderFlags(pipeline_options);
    }
}

void glTFRenderPassManager::RenderAllPass(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs) const
{
    for (const auto& pass : m_passes)
    {
        pass->PreRenderPass(resource_manager);
        pass->RenderPass(resource_manager);
        pass->PostRenderPass(resource_manager);
    }
}

void glTFRenderPassManager::RenderEnd(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs)
{
    RHIUtils::Instance().AddRenderTargetBarrierToCommandList(
        resource_manager.GetCommandListForRecord(),
        resource_manager.GetCurrentFrameSwapchainRT(),
        RHIResourceStateType::STATE_RENDER_TARGET, RHIResourceStateType::STATE_PRESENT);

    // TODO: no waiting causing race with base color and normal?
    resource_manager.CloseCommandListAndExecute(true);
    RHIUtils::Instance().Present(resource_manager.GetSwapchain());
    
    resource_manager.UpdateCurrentBackBufferIndex();
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}

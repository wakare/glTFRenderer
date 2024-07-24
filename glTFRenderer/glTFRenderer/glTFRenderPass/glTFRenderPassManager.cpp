#include "glTFRenderPassManager.h"

#include <imgui.h>

#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "glTFRHI/RHIUtils.h"
#include "RenderWindow/glTFWindow.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"

glTFRenderPassManager::glTFRenderPassManager()
    : m_frame_index(0)
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
    // Generate render pass before initialize sub pass
    /*
    if (!m_render_pass)
    {
        RHIRenderPassInfo create_render_pass_info;
        for (const auto& pass : m_passes)
        {
            
        }
        
        m_render_pass = RHIResourceFactory::CreateRHIResource<IRHIRenderPass>();
        m_render_pass->InitRenderPass(resource_manager.GetDevice(), create_render_pass_info);
    }
    */

    // Create pass resource and relocation
    for (const auto& pass : m_passes)
    {
        pass->InitResourceTable(resource_manager);
    }
    
    for (const auto& pass : m_passes)
    {
        pass->ExportResourceLocation(resource_manager);
    }
    
    for (const auto& pass : m_passes)
    {
        pass->ImportResourceLocation(resource_manager);
    }

    for (const auto& pass : m_passes)
    {
        const bool inited = pass->InitRenderInterface(resource_manager) && pass->InitPass(resource_manager);
        GLTF_CHECK(inited);
        LOG_FORMAT("[DEBUG] Init pass %s finished!\n", pass->PassName())
    }

    resource_manager.CloseCommandListAndExecute({}, true);
    
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(glTFRenderResourceManager& resource_manager, const glTFSceneView& scene_view, size_t deltaTimeMs)
{
    // Gather all scene pass
    
    std::vector<const glTFSceneNode*> dirty_objects;
    scene_view.TraverseSceneObjectWithinView([this, &dirty_objects](const glTFSceneNode& node)
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

        if (auto* scene_view_interface = pass->GetRenderInterface<glTFRenderInterfaceSceneView>())
        {
            scene_view_interface->UpdateSceneView(resource_manager, scene_view);
        }

        if (auto* frame_stat = pass->GetRenderInterface<glTFRenderInterfaceFrameStat>())
        {
            frame_stat->UploadCPUBuffer(resource_manager, &m_frame_index, 0, sizeof(m_frame_index));
        }
    }
    ++m_frame_index;
}

void glTFRenderPassManager::UpdateAllPassGUIWidgets()
{
    ImGui::Separator();
    ImGui::Dummy({10.0f, 10.0f});
    ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "Pass Config");
    
    for (const auto& pass : m_passes)
    {
        ImGui::Dummy({10.0f, 10.0f});
        if (ImGui::TreeNodeEx(pass->PassName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            pass->UpdateGUIWidgets();
            ImGui::TreePop();
        }
    }
    
    ImGui::Separator();
}

void glTFRenderPassManager::RenderBegin(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs)
{
    resource_manager.WaitLastFrameFinish();
    
    // Reset command allocator when previous frame executed finish...
    resource_manager.ResetCommandAllocator();

    // Wait current frame available
    resource_manager.GetSwapChain().AcquireNewFrame(resource_manager.GetDevice());
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
    auto& command_list = resource_manager.GetCommandListForRecord();
    resource_manager.GetCurrentFrameSwapChainRTV().m_source->Transition(command_list, RHIResourceStateType::STATE_PRESENT);

    RHIExecuteCommandListContext context;
    context.wait_infos.push_back({&resource_manager.GetSwapChain().GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
    context.sign_semaphores.push_back(&command_list.GetSemaphore());
    
    // TODO: no waiting causing race with base color and normal?
    resource_manager.CloseCommandListAndExecute(context, true);
    RHIUtils::Instance().Present(resource_manager.GetSwapChain(), resource_manager.GetCommandQueue(), command_list);
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}

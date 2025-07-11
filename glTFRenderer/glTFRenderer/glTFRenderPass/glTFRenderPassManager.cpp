#include "glTFRenderPassManager.h"

#include <imgui.h>

#include "glTFRenderMaterialManager.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderInterface/glTFRenderInterfaceFrameStat.h"
#include "RHIUtils.h"
#include "RenderWindow/glTFWindow.h"
#include "RHIResourceFactoryImpl.hpp"

glTFRenderPassManager::glTFRenderPassManager()
    : m_frame_index(0)
{
}
 
bool glTFRenderPassManager::InitRenderPassManager(glTFRenderResourceManager& resource_manager)
{
    return true;
}

void glTFRenderPassManager::AddRenderPass(RenderPassPhaseType type, std::shared_ptr<glTFRenderPassBase> pass)
{
    m_passes[type].push_back(std::move(pass));
}

void glTFRenderPassManager::InitAllPass(glTFRenderResourceManager& resource_manager)
{
    // Generate render pass before initialize sub pass
    // Create pass resource and relocation
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        pass.InitResourceTable(resource_manager);
    });
    
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        pass.ExportResourceLocation(resource_manager);
    });
    
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        pass.ImportResourceLocation(resource_manager);
    });

    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        const bool inited = pass.InitRenderInterface(resource_manager) && pass.InitPass(resource_manager);
        GLTF_CHECK(inited);
        LOG_FORMAT("[DEBUG] Init pass %s finished!\n", pass.PassName())
    });
    
    resource_manager.CloseCurrentCommandListAndExecute({}, true);
    LOG_FORMAT_FLUSH("[DEBUG] Init all pass finished!\n")
}

void glTFRenderPassManager::UpdateScene(glTFRenderResourceManager& resource_manager, const glTFSceneView& scene_view, size_t delta_time_ms)
{
    std::vector<const glTFSceneNode*> dirty_objects = scene_view.GetDirtySceneNodes();

    for (const auto* node : dirty_objects)
    {
        for (const auto& scene_object : node->m_objects)
        {
            TraveseAllPass([&](glTFRenderPassBase& pass)
            {
                pass.TryProcessSceneObject(resource_manager, *scene_object);
            });
        }
    }
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        const bool success = pass.FinishProcessSceneObject(resource_manager);
        GLTF_CHECK(success);

        if (auto* frame_stat = pass.GetRenderInterface<glTFRenderInterfaceFrameStat>())
        {
            frame_stat->UploadBuffer(resource_manager, &m_frame_index, 0, sizeof(m_frame_index));
        }
    });
    
    ++m_frame_index;
}

void glTFRenderPassManager::UpdateAllPassGUIWidgets()
{
    ImGui::Separator();
    ImGui::Dummy({10.0f, 10.0f});
    ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "Pass Config");
    
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        ImGui::Dummy({10.0f, 10.0f});
        if (ImGui::TreeNodeEx(pass.PassName(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            pass.UpdateGUIWidgets();
            ImGui::TreePop();
        }
    });
    
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
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        pass.UpdateRenderFlags(pipeline_options);
    });
}

void glTFRenderPassManager::RenderAllPass(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs) const
{
    TraveseAllPass([&](glTFRenderPassBase& pass)
    {
        if (pass.IsRenderingEnabled())
        {
            pass.UpdateFrameIndex(resource_manager);
            pass.ModifyFinalOutput(resource_manager.GetFinalOutput());
        
            pass.PreRenderPass(resource_manager);
            pass.RenderPass(resource_manager);
            pass.PostRenderPass(resource_manager);            
        }
    });
    
    auto& command_list = resource_manager.GetCommandListForRecord();
    // Copy compute result to swapchain back buffer
    if (resource_manager.GetFinalOutput().final_color_output)
    {
        resource_manager.GetCurrentFrameSwapChainTexture().Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
        resource_manager.GetFinalOutput().final_color_output->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
        bool copy = RHIUtilInstanceManager::Instance().CopyTexture(command_list,
                                                     resource_manager.GetCurrentFrameSwapChainTexture(),
                                                     *resource_manager.GetFinalOutput().final_color_output, {});
        GLTF_CHECK(copy);
    }
}

void glTFRenderPassManager::RenderEnd(glTFRenderResourceManager& resource_manager, size_t deltaTimeMs)
{
    auto& command_list = resource_manager.GetCommandListForRecord();
    resource_manager.GetCurrentFrameSwapChainRTV().m_source->Transition(command_list, RHIResourceStateType::STATE_PRESENT);

    RHIExecuteCommandListContext context;
    context.wait_infos.push_back({&resource_manager.GetSwapChain().GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
    context.sign_semaphores.push_back(&command_list.GetSemaphore());
    
    resource_manager.CloseCurrentCommandListAndExecute(context, false);
    
    RHIUtilInstanceManager::Instance().Present(resource_manager.GetSwapChain(), resource_manager.GetCommandQueue(), resource_manager.GetCommandListForRecord());
    resource_manager.CloseCurrentCommandListAndExecute({}, false);
}

void glTFRenderPassManager::ExitAllPass()
{
    m_passes.clear();
}

void glTFRenderPassManager::TraveseAllPass(std::function<void(glTFRenderPassBase& pass)> lambda) const
{
    for (auto& pass_info : m_passes)
    {
        for (auto& pass : pass_info.second)
        {
            lambda(*pass);
        }
    }
}

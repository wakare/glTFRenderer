#include "glTFAppRenderPipeline.h"

#include <imgui.h>

#include "glTFGUI/glTFGUI.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassIndirectDrawCulling.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassLighting.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassRayTracingPostprocess.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassReSTIRDirectLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshDepth.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassPathTracing.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassReSTIRDirectLighting.h"
#include "glTFWindow/glTFWindow.h"

glTFAppRenderPipelineBase::glTFAppRenderPipelineBase()
    : m_render_pass_dirty(true)
{
    m_pass_options.SetEnableLit(true);
    m_pass_options.SetEnableCulling(true);
}

void glTFAppRenderPipelineBase::TickFrameRenderingBegin(glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    m_pass_manager->RenderBegin(resource_manager, delta_time_ms);
}

void glTFAppRenderPipelineBase::TickSceneRendering(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    if (m_render_pass_dirty)
    {
        const bool created = RecreateRenderPass(resource_manager);
        GLTF_CHECK(created);
        m_render_pass_dirty = false;
    }

    m_pass_manager->UpdatePipelineOptions(m_pass_options);
    m_pass_manager->UpdateScene(resource_manager, scene_view, delta_time_ms);
    m_pass_manager->RenderAllPass(resource_manager, delta_time_ms);
}

void glTFAppRenderPipelineBase::TickGUIWidgetUpdate(glTFGUI& GUI, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    UpdateGUIWidgets();
    
    // Setup all widgets for render passes
    m_pass_manager->UpdateAllPassGUIWidgets();
}

void glTFAppRenderPipelineBase::TickFrameRenderingEnd(glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    m_pass_manager->RenderEnd(resource_manager, delta_time_ms);
}

void glTFAppRenderPipelineBase::ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms)
{
    if (input_manager.IsKeyPressed(GLFW_KEY_C))
    {
        m_pass_options.SetEnableCulling(true);
    }

    if (input_manager.IsKeyPressed(GLFW_KEY_V))
    {
        m_pass_options.SetEnableCulling(false);
    }
}

bool glTFAppRenderPipelineBase::RecreateRenderPass(glTFRenderResourceManager& resource_manager)
{
    m_pass_manager.reset(new glTFRenderPassManager());
    SetupRenderPipeline();
    
    m_pass_manager->InitRenderPassManager(glTFWindow::Get());
    
    m_pass_manager->InitAllPass(resource_manager);
    
    return true;
}

bool glTFAppRenderPipelineRasterScene::SetupRenderPipeline()
{
    std::unique_ptr<glTFComputePassIndirectDrawCulling> culling_pass = std::make_unique<glTFComputePassIndirectDrawCulling>();
    m_pass_manager->AddRenderPass(std::move(culling_pass));
        
    std::unique_ptr<glTFGraphicsPassMeshDepth> depth_pass = std::make_unique<glTFGraphicsPassMeshDepth>();
    m_pass_manager->AddRenderPass(std::move(depth_pass));

    std::unique_ptr<glTFGraphicsPassMeshOpaque> opaque_pass = std::make_unique<glTFGraphicsPassMeshOpaque>();
    m_pass_manager->AddRenderPass(std::move(opaque_pass));

    std::unique_ptr<glTFComputePassLighting> compute_lighting_pass = std::make_unique<glTFComputePassLighting>();
    m_pass_manager->AddRenderPass(std::move(compute_lighting_pass));

    return true;
}

glTFAppRenderPipelineRayTracingScene::glTFAppRenderPipelineRayTracingScene(bool use_restir_direct_lighting)
    : m_use_restir_direct_lighting(use_restir_direct_lighting)
{
}

bool glTFAppRenderPipelineRayTracingScene::SetupRenderPipeline()
{
    if (m_use_restir_direct_lighting)
    {
        std::unique_ptr<glTFRayTracingPassReSTIRDirectLighting> restir_lighting_samples_pass = std::make_unique<glTFRayTracingPassReSTIRDirectLighting>();
        m_pass_manager->AddRenderPass(std::move(restir_lighting_samples_pass));

        std::unique_ptr<glTFComputePassReSTIRDirectLighting> restir_lighting_pass = std::make_unique<glTFComputePassReSTIRDirectLighting>();
        m_pass_manager->AddRenderPass(std::move(restir_lighting_pass));
    }
    else
    {
        // fallback to path tracing
        std::unique_ptr<glTFRayTracingPassPathTracing> ray_tracing_main = std::make_unique<glTFRayTracingPassPathTracing>();
        m_pass_manager->AddRenderPass(std::move(ray_tracing_main));
    }
    
    std::unique_ptr<glTFComputePassRayTracingPostprocess> ray_tracing_postprocess = std::make_unique<glTFComputePassRayTracingPostprocess>();
    m_pass_manager->AddRenderPass(std::move(ray_tracing_postprocess));

    return true;
}

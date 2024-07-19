#include "glTFSceneRenderer.h"

#include <imgui.h>

#include "glTFGUIRenderer.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassIndirectDrawCulling.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassLighting.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassRayTracingPostprocess.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassReSTIRDirectLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshDepth.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassPathTracing.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassReSTIRDirectLighting.h"
#include "glTFRenderPass/glTFTestPass/glTFGraphicsPassTestIndexedTriangle.h"
#include "glTFRenderPass/glTFTestPass/glTFGraphicsPassTestTriangleSimplest.h"
#include "RenderWindow//glTFWindow.h"

glTFSceneRendererBase::glTFSceneRendererBase()
{
    m_pass_options.SetEnableLit(true);
    m_pass_options.SetEnableCulling(true);
}

glTFSceneRendererBase::~glTFSceneRendererBase()
{
    if (m_pass_manager)
    {
        m_pass_manager->ExitAllPass();
    }
}

void glTFSceneRendererBase::TickFrameRenderingBegin(glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    m_pass_manager->RenderBegin(resource_manager, delta_time_ms);
}

void glTFSceneRendererBase::TickSceneRendering(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    if (!m_pass_inited)
    {
        const bool created = RecreateRenderPass(resource_manager);
        GLTF_CHECK(created);
        m_pass_inited = true;
    }

    m_pass_manager->UpdatePipelineOptions(m_pass_options);
    m_pass_manager->UpdateScene(resource_manager, scene_view, delta_time_ms);
    m_pass_manager->RenderAllPass(resource_manager, delta_time_ms);
}

void glTFSceneRendererBase::TickGUIWidgetUpdate(glTFGUIRenderer& GUI, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    UpdateGUIWidgets();
    
    // Setup all widgets for render passes
    m_pass_manager->UpdateAllPassGUIWidgets();
}

void glTFSceneRendererBase::TickFrameRenderingEnd(glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    m_pass_manager->RenderEnd(resource_manager, delta_time_ms);
}

void glTFSceneRendererBase::ApplyInput(const glTFInputManager& input_manager, size_t delta_time_ms)
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

bool glTFSceneRendererBase::RecreateRenderPass(glTFRenderResourceManager& resource_manager)
{
    //resource_manager.GetMemoryManager().CleanAllocatedResource();
    //resource_manager.InitMemoryManager();

    m_pass_manager.reset(new glTFRenderPassManager());
    SetupSceneRenderer();
    
    m_pass_manager->InitRenderPassManager(glTFWindow::Get());
    m_pass_manager->InitAllPass(resource_manager);
    
    return true;
}

bool glTFSceneRendererRasterizer::SetupSceneRenderer()
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

glTFSceneRendererRayTracer::glTFSceneRendererRayTracer(bool use_restir_direct_lighting)
    : m_use_restir_direct_lighting(use_restir_direct_lighting)
{
}

bool glTFSceneRendererRayTracer::SetupSceneRenderer()
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

bool glTFSceneRendererTestTriangle::SetupSceneRenderer()
{
    //std::unique_ptr<glTFGraphicsPassTestTriangleSimplest> test_triangle_pass = std::make_unique<glTFGraphicsPassTestTriangleSimplest>();
    std::unique_ptr<glTFGraphicsPassTestIndexedTriangle> test_triangle_pass = std::make_unique<glTFGraphicsPassTestIndexedTriangle>();
    m_pass_manager->AddRenderPass(std::move(test_triangle_pass));

    return true;
}

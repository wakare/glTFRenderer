#include "glTFSceneRenderer.h"

#include <imgui.h>

#include "glTFGUIRenderer.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassIndirectDrawCulling.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassLighting.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassRayTracingPostprocess.h"
#include "glTFRenderPass/glTFComputePass/glTFComputePassReSTIRDirectLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshDepth.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshShadowDepth.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassPathTracing.h"
#include "glTFRenderPass/glTFRayTracingPass/glTFRayTracingPassReSTIRDirectLighting.h"
#include "glTFRenderPass/glTFTestPass/glTFComputePassTestFillColor.h"
#include "glTFRenderPass/glTFTestPass/glTFGraphicsPassTestIndexedTextureTriangle.h"
#include "glTFRenderPass/glTFTestPass/glTFGraphicsPassTestIndexedTriangle.h"
#include "glTFRenderPass/glTFTestPass/glTFGraphicsPassTestSceneRendering.h"
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

void glTFSceneRendererBase::TickSceneUpdating(const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
}

void glTFSceneRendererBase::TickSceneRendering(glTFRenderResourceManager& resource_manager, const glTFSceneView& scene_view, const glTFSceneGraph& scene_graph, size_t delta_time_ms)
{
    if (!m_pass_inited)
    {
        const bool created = RecreateRenderPass(resource_manager, scene_graph);
        GLTF_CHECK(created);
        m_pass_inited = true;
    }
    
    m_pass_manager->UpdateScene(resource_manager, scene_view, delta_time_ms);
    m_pass_manager->UpdatePipelineOptions(m_pass_options);
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

bool glTFSceneRendererBase::RecreateRenderPass(glTFRenderResourceManager& resource_manager, const glTFSceneGraph& scene_graph)
{
    m_pass_manager.reset(new glTFRenderPassManager());
    
    SetupSceneRenderer(scene_graph);
    for (const auto& render_system : resource_manager.GetRenderSystems())
    {
        render_system->SetupPass(*m_pass_manager, scene_graph);
    }
    
    m_pass_manager->InitRenderPassManager(resource_manager);
    m_pass_manager->InitAllPass(resource_manager);
    
    return true;
}

bool glTFSceneRendererRasterizer::SetupSceneRenderer(const glTFSceneGraph& scene_graph)
{
    m_pass_manager->AddRenderPass(PRE_SCENE, std::make_shared<glTFComputePassIndirectDrawCulling>());
    m_pass_manager->AddRenderPass(SCENE_DEPTH, std::make_shared<glTFGraphicsPassMeshDepth>());
    m_pass_manager->AddRenderPass(SCENE_BASE_PASS, std::make_shared<glTFGraphicsPassMeshOpaque>());
    m_pass_manager->AddRenderPass(SCENE_LIGHTING, std::make_shared<glTFComputePassLighting>());
    
    return true;
}

glTFSceneRendererRayTracer::glTFSceneRendererRayTracer(bool use_restir_direct_lighting)
    : m_use_restir_direct_lighting(use_restir_direct_lighting)
{
}

bool glTFSceneRendererRayTracer::SetupSceneRenderer(const glTFSceneGraph& scene_graph)
{
    if (m_use_restir_direct_lighting)
    {
        m_pass_manager->AddRenderPass(SCENE_LIGHTING, std::make_shared<glTFRayTracingPassReSTIRDirectLighting>());
        m_pass_manager->AddRenderPass(SCENE_LIGHTING, std::make_shared<glTFComputePassReSTIRDirectLighting>());
    }
    else
    {
        // fallback to path tracing
        m_pass_manager->AddRenderPass(SCENE_LIGHTING, std::make_shared<glTFRayTracingPassPathTracing>());
    }
    
    m_pass_manager->AddRenderPass(POST_SCENE, std::make_shared<glTFComputePassRayTracingPostprocess>());

    return true;
}

bool glTFSceneRendererTestTriangle::SetupSceneRenderer(const glTFSceneGraph& scene_graph)
{
    //std::unique_ptr<glTFGraphicsPassTestTriangleSimplest> test_triangle_pass = std::make_unique<glTFGraphicsPassTestTriangleSimplest>();
    //std::unique_ptr<glTFGraphicsPassTestIndexedTriangle> test_triangle_pass = std::make_unique<glTFGraphicsPassTestIndexedTriangle>();
    //std::unique_ptr<glTFGraphicsPassTestIndexedTextureTriangle> test_triangle_pass = std::make_unique<glTFGraphicsPassTestIndexedTextureTriangle>();
    //std::unique_ptr<glTFGraphicsPassTestSceneRendering> test_triangle_pass = std::make_unique<glTFGraphicsPassTestSceneRendering>();
    m_pass_manager->AddRenderPass(PRE_SCENE, std::make_shared<glTFComputePassTestFillColor>());

    return true;
}

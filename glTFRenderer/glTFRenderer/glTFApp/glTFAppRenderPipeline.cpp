#include "glTFAppRenderPipeline.h"

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

void glTFAppRenderPipelineBase::TickSceneRendering(const glTFSceneGraph& scene_graph, const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    if (m_render_pass_dirty)
    {
        const bool created = RecreateRenderPass(scene_graph, scene_view, resource_manager);
        GLTF_CHECK(created);
        m_render_pass_dirty = false;
    }

    const bool processed = ProcessDirtySceneObject(scene_graph);
    GLTF_CHECK(processed);

    m_pass_manager->UpdatePipelineOptions(m_pass_options);
    m_pass_manager->UpdateScene(resource_manager, delta_time_ms);
    m_pass_manager->RenderAllPass(resource_manager, delta_time_ms);
}

void glTFAppRenderPipelineBase::TickGUIRendering(glTFGUI& GUI, glTFRenderResourceManager& resource_manager, size_t delta_time_ms)
{
    GUI.RenderNewFrame(resource_manager);
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

bool glTFAppRenderPipelineBase::RecreateRenderPass(const glTFSceneGraph& scene_graph, const glTFSceneView& scene_view, glTFRenderResourceManager& resource_manager)
{
    VertexLayoutDeclaration resolved_vertex_layout;
    bool has_resolved = false; 
    
    scene_graph.TraverseNodes([&resolved_vertex_layout, &has_resolved](const glTFSceneNode& node)
    {
        for (auto& scene_object : node.m_objects)
        {
            if (auto* primitive = dynamic_cast<glTFScenePrimitive*>(scene_object.get()))
            {
                if (!has_resolved)
                {
                    for (const auto& vertex_layout : primitive->GetVertexLayout().elements)
                    {
                        if (!resolved_vertex_layout.HasAttribute(vertex_layout.type))
                        {
                            resolved_vertex_layout.elements.push_back(vertex_layout);
                            has_resolved = true;
                        }
                    }    
                }
	            
                else if (!(primitive->GetVertexLayout() == resolved_vertex_layout))
                {
                    LOG_FORMAT_FLUSH("[WARN] Primtive id: %d is no-visible becuase vertex layout mismatch\n", primitive->GetID())
                    primitive->SetVisible(false);
                }
            }    
        }
	    
        return true;
    });

    GLTF_CHECK(has_resolved);
    
    m_pass_manager.reset(new glTFRenderPassManager(scene_view));
    SetupRenderPipeline();
    
    m_pass_manager->InitRenderPassManager(glTFWindow::Get());
    RETURN_IF_FALSE(resource_manager.GetMeshManager().ResolveVertexInputLayout(resolved_vertex_layout))
    
    m_pass_manager->InitAllPass(resource_manager);
    
    return true;
}

bool glTFAppRenderPipelineBase::ProcessDirtySceneObject(const glTFSceneGraph& scene_graph)
{
    // TODO: Dynamic update scene objects
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

glTFAppRenderPipelineRayTracingScene::glTFAppRenderPipelineRayTracingScene()
    : use_restir_direct_lighting(true)
{
}

bool glTFAppRenderPipelineRayTracingScene::SetupRenderPipeline()
{
    if (use_restir_direct_lighting)
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
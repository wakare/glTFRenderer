#include "glTFAppRenderer.h"
#include "glTFGUI/glTFGUI.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassMeshOpaque.h"
#include "glTFWindow/glTFWindow.h"

glTFAppRenderer::glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window)
{
    RHIConfigSingleton::Instance().SetGraphicsAPIType(renderer_config.Vulkan ?
        RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);
    
    if (renderer_config.raster)
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRasterScene);    
    }
    else
    {
        m_render_pipeline.reset(new glTFAppRenderPipelineRayTracingScene(renderer_config.ReSTIR));
    }

    m_resource_manager.reset(new glTFRenderResourceManager());
    m_resource_manager->InitResourceManager(window.GetWidth(), window.GetHeight(), window.GetHWND());
}

bool glTFAppRenderer::InitGUIContext(glTFGUI& GUI, const glTFWindow& window) const
{
    return GUI.SetupGUIContext(window, *m_resource_manager);
}

bool glTFAppRenderer::InitMeshResourceWithSceneGraph(const glTFSceneGraph& scene_graph)
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
    
    m_resource_manager->GetMeshManager().ResolveVertexInputLayout(resolved_vertex_layout);
    
    m_scene_view.reset(new glTFSceneView(scene_graph));    
    m_scene_view->TraverseSceneObjectWithinView([&](const glTFSceneNode& node)
       {
           for (const auto& scene_object : node.m_objects)
           {
               m_resource_manager->TryProcessSceneObject(*m_resource_manager, *scene_object);    
           }
        
           return true;
       });
    
    GLTF_CHECK(m_resource_manager->GetMeshManager().BuildMeshRenderResource(*m_resource_manager));
    
    return true;
}

void glTFAppRenderer::TickRenderingBegin(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingBegin(*m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneUpdating(const glTFSceneGraph& scene_graph, size_t delta_time_ms)
{
    m_scene_view->Tick(scene_graph);
}

void glTFAppRenderer::TickSceneRendering(const glTFInputManager& input_manager, size_t delta_time_ms)
{
    input_manager.TickSceneView(*m_scene_view, delta_time_ms);
    input_manager.TickRenderPipeline(*m_render_pipeline, delta_time_ms);
    
    m_render_pipeline->TickSceneRendering(*m_scene_view, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickGUIWidgetUpdate(glTFGUI& GUI, size_t delta_time_ms)
{
    m_render_pipeline->TickGUIWidgetUpdate(GUI, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickRenderingEnd(size_t delta_time_ms)
{
    m_render_pipeline->TickFrameRenderingEnd(*m_resource_manager, delta_time_ms);
}

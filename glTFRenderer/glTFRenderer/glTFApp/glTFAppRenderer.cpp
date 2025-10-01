#include "glTFAppRenderer.h"
#include "glTFGUIRenderer.h"
#include "glTFRenderPass/glTFGraphicsPass/glTFGraphicsPassLighting.h"
#include "glTFRenderSystem/Shadow/ShadowRenderSystem.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "RHIConfigSingleton.h"
#include "RHIResourceFactory.h"
#include "RHIUtils.h"
#include "RenderWindow/glTFWindow.h"

glTFAppRenderer::glTFAppRenderer(const glTFAppRendererConfig& renderer_config, const glTFWindow& window)
{
    RHIConfigSingleton::Instance().SetGraphicsAPIType(renderer_config.vulkan ?
        RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);
    
    RHIConfigSingleton::Instance().InitGraphicsAPI();
    
    if (renderer_config.test_triangle)
    {
        m_scene_renderer.reset(new glTFSceneRendererTestTriangle);   
    }
    else
    {
        if (renderer_config.raster)
        {
            m_scene_renderer.reset(new glTFSceneRendererRasterizer);    
        }
        else
        {
            m_scene_renderer.reset(new glTFSceneRendererRayTracer(renderer_config.ReSTIR));
        }    
    }

    RHIUtilInstanceManager::Instance().ReportLiveObjects();
    
    m_resource_manager.reset(new glTFRenderResourceManager());

    m_resource_manager->InitResourceManager(window.GetWidth(), window.GetHeight(), window.GetHWND());

    // Init render systems
    if (renderer_config.shadow)
    {
        auto shadow_system = std::make_shared<ShadowRenderSystem>();
        m_render_systems.push_back(shadow_system);
        m_resource_manager->AddRenderSystem(shadow_system);
    }

    if (renderer_config.virtual_texture)
    {
        auto vt_system = std::make_shared<VirtualTextureSystem>();
        m_render_systems.push_back(vt_system);
        m_resource_manager->AddRenderSystem(vt_system);
    }
    
    m_resource_manager->InitRenderSystems();
    
    if (renderer_config.ui)
    {
        m_ui_renderer = std::make_unique<glTFGUIRenderer>();
        m_ui_renderer->SetupGUIContext(glTFWindow::Get(), GetResourceManager());
        m_ui_renderer->AddWidgetSetupCallback([this]()
        {
            m_scene_renderer->TickGUIWidgetUpdate(*m_ui_renderer, *m_resource_manager, 0);
        });    
    }
}

bool glTFAppRenderer::InitScene(std::shared_ptr<glTFSceneGraph> scene_graph)
{
    m_scene_view.reset(new glTFSceneView(scene_graph));
    return m_resource_manager->InitScene(*scene_graph);
}

void glTFAppRenderer::TickRenderingBegin(size_t delta_time_ms)
{
    m_scene_renderer->TickFrameRenderingBegin(*m_resource_manager, delta_time_ms);
    m_resource_manager->TickFrame();
}

void glTFAppRenderer::TickSceneUpdating(const glTFSceneGraph& scene_graph, const RendererInputDevice& input_manager, size_t delta_time_ms)
{
    m_scene_view->Tick(scene_graph);
    m_scene_view->ApplyInput(input_manager, delta_time_ms);
    m_scene_view->GatherDirtySceneNodes();

    m_resource_manager->TickSceneUpdating(*m_scene_view, *m_resource_manager, scene_graph, delta_time_ms);
    
    m_scene_renderer->ApplyInput(input_manager, delta_time_ms);
    m_scene_renderer->TickSceneUpdating(*m_scene_view, *m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::TickSceneRendering(const RendererInputDevice& input_manager, const glTFSceneGraph& scene_graph, size_t delta_time_ms)
{
    m_scene_renderer->TickSceneRendering(*m_resource_manager, *m_scene_view, scene_graph, delta_time_ms);
    for (const auto& render_system : m_render_systems)
    {
        render_system->TickRenderSystem(*m_resource_manager);
    }   
}

void glTFAppRenderer::TickGUIWidgetUpdate(size_t delta_time_ms)
{
    if (!m_ui_renderer)
    {
        return;
    }
    
    m_ui_renderer->UpdateWidgets();
    m_ui_renderer->RenderWidgets(*m_resource_manager);
}

void glTFAppRenderer::TickRenderingEnd(size_t delta_time_ms)
{
    m_scene_renderer->TickFrameRenderingEnd(*m_resource_manager, delta_time_ms);
}

void glTFAppRenderer::WaitRenderingFinishAndCleanupAllResource() const
{
    m_resource_manager->WaitAllFrameFinish();
    m_resource_manager->WaitPresentFinished();
    
    if (m_ui_renderer)
    {
        m_ui_renderer->ExitAndClean();    
    }

    m_resource_manager->WaitAndClean();
    
    const bool cleanup = RHIResourceFactory::CleanupResources(m_resource_manager->GetMemoryManager());
    GLTF_CHECK(cleanup);
}

glTFGUIRenderer& glTFAppRenderer::GetGUIRenderer() const
{
    return *m_ui_renderer;
}

#include "glTFRenderResourceManager.h"

#include "glTFRenderMaterialManager.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "glTFWindow/glTFWindow.h"

#define GLFW_EXPOSE_NATIVE_WIN32 1
#include <GLFW/glfw3native.h>

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

constexpr size_t backBufferCount = 3;

std::shared_ptr<IRHIFactory> glTFRenderResourceManager::m_factory = nullptr;
std::shared_ptr<IRHIDevice> glTFRenderResourceManager::m_device = nullptr;
std::shared_ptr<IRHICommandQueue> glTFRenderResourceManager::m_command_queue = nullptr;
std::shared_ptr<IRHISwapChain> glTFRenderResourceManager::m_swap_chain = nullptr;

glTFRenderResourceManager::glTFRenderResourceManager()
    : m_radiosity_renderer(std::make_shared<glTFRadiosityRenderer>())
    , m_material_manager(std::make_shared<glTFRenderMaterialManager>())
    , m_mesh_manager(std::make_shared<glTFRenderMeshManager>())
    , m_GBuffer_allocations(new glTFRenderResourceUtils::GBufferSignatureAllocations)
{
}

bool glTFRenderResourceManager::InitResourceManager(unsigned width, unsigned height, HWND handle)
{
    if (!m_factory)
    {
        m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
        EXIT_WHEN_FALSE(m_factory->InitFactory())    
    }
    
    if (!m_device)
    {
        m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
        EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))    
    }
    
    if (!m_command_queue)
    {
        m_command_queue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
        EXIT_WHEN_FALSE(m_command_queue->InitCommandQueue(*m_device))
    }

    if (!m_swap_chain)
    {
        m_swap_chain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
        EXIT_WHEN_FALSE(m_swap_chain->InitSwapChain(*m_factory, *m_device, *m_command_queue, width, height, false, handle))    
    }
    
    m_command_allocators.resize(backBufferCount);
    m_command_lists.resize(backBufferCount);
    m_command_list_record_state.resize(backBufferCount);
    
    for (size_t i = 0; i < backBufferCount; ++i)
    {
        m_command_allocators[i] = RHIResourceFactory::CreateRHIResource<IRHICommandAllocator>();
        m_command_allocators[i]->InitCommandAllocator(*m_device, RHICommandAllocatorType::DIRECT);
        
        m_command_lists[i] = RHIResourceFactory::CreateRHIResource<IRHICommandList>();
        m_command_lists[i]->InitCommandList(*m_device, *m_command_allocators[i]);
        m_command_list_record_state[i] = false;
    }

    m_render_target_manager = RHIResourceFactory::CreateRHIResource<IRHIRenderTargetManager>();
    m_render_target_manager->InitRenderTargetManager(*m_device, 100);

    RHIRenderTargetClearValue clearValue;
    clearValue.clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB;
    clearValue.clear_color = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *m_swap_chain, clearValue);

    RHIRenderTargetClearValue depth_clear_value{};
    depth_clear_value.clear_format = RHIDataFormat::D32_FLOAT;
    depth_clear_value.clearDS.clear_depth = 1.0f;
    depth_clear_value.clearDS.clear_stencil_value = 0;
    m_depth_texture = m_render_target_manager->CreateRenderTarget(*m_device, RHIRenderTargetType::DSV, RHIDataFormat::R32_TYPELESS,
                                                               RHIDataFormat::D32_FLOAT, RHIRenderTargetDesc{GetSwapChain().GetWidth(), GetSwapChain().GetHeight(), false, depth_clear_value, "ResourceManager_DepthRT"});

    m_frame_resource_managers.resize(backBufferCount);
    
    return true;
}

bool glTFRenderResourceManager::InitScene(const glTFSceneGraph& scene_graph)
{
    VertexLayoutDeclaration resolved_vertex_layout;
    bool has_resolved = false; 
    
    scene_graph.TraverseNodes([&resolved_vertex_layout, &has_resolved, this](const glTFSceneNode& node)
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
            TryProcessSceneObject(*this, *scene_object);
        }
	    
        return true;
    });

    GLTF_CHECK(has_resolved);
    GetMeshManager().ResolveVertexInputLayout(resolved_vertex_layout);
    
    GLTF_CHECK(GetMeshManager().BuildMeshRenderResource(*this));
    m_radiosity_renderer->InitScene(scene_graph);
    
    return true;
}

IRHIFactory& glTFRenderResourceManager::GetFactory()
{
    return *m_factory;
}

IRHIDevice& glTFRenderResourceManager::GetDevice()
{
    return *m_device;
}

IRHISwapChain& glTFRenderResourceManager::GetSwapChain()
{
    return *m_swap_chain;
}

IRHICommandQueue& glTFRenderResourceManager::GetCommandQueue()
{
    return *m_command_queue;
}

IRHICommandList& glTFRenderResourceManager::GetCommandListForRecord()
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % backBufferCount;
    auto& command_list = *m_command_lists[current_frame_index];
    auto& command_allocator = *m_command_allocators[current_frame_index];
    if (!m_command_list_record_state[current_frame_index])
    {
        const bool reset = RHIUtils::Instance().ResetCommandList(command_list, command_allocator, m_current_pass_pso.get());
        GLTF_CHECK(reset);
        
        m_command_list_record_state[current_frame_index] = true;
    }
    
    return command_list;
}

void glTFRenderResourceManager::CloseCommandListAndExecute(bool wait)
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % backBufferCount;
    if (!m_command_list_record_state[current_frame_index])
    {
        return;
    }
    
    auto& command_list = *m_command_lists[current_frame_index];
    
    const bool closed = RHIUtils::Instance().CloseCommandList(command_list);
    GLTF_CHECK(closed);

    RHIExecuteCommandListContext context;
    context.wait_infos.push_back({&m_swap_chain->GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
    context.sign_semaphores.push_back(&command_list.GetSemaphore());
    
    GLTF_CHECK(RHIUtils::Instance().ExecuteCommandList(command_list, GetCommandQueue(), context));
    if (wait)
    {
        RHIUtils::Instance().WaitCommandListFinish(command_list);
    }
    
    m_command_list_record_state[current_frame_index] = false;
}

void glTFRenderResourceManager::WaitAllFrameFinish() const
{
    for (const auto& command_list : m_command_lists)
    {
        RHIUtils::Instance().WaitCommandListFinish(*command_list);
    }
}

void glTFRenderResourceManager::ResetCommandAllocator()
{
    CloseCommandListAndExecute(true);
    RHIUtils::Instance().ResetCommandAllocator(GetCurrentFrameCommandAllocator());
}

IRHIRenderTargetManager& glTFRenderResourceManager::GetRenderTargetManager()
{
    return *m_render_target_manager;
}

IRHICommandAllocator& glTFRenderResourceManager::GetCurrentFrameCommandAllocator()
{
    return *m_command_allocators[GetCurrentBackBufferIndex() % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetCurrentFrameSwapChainRT()
{
    return *m_swapchain_RTs[GetCurrentBackBufferIndex() % backBufferCount];
}

IRHIRenderTarget& glTFRenderResourceManager::GetDepthRT()
{
    return *m_depth_texture;
}

glTFRenderMaterialManager& glTFRenderResourceManager::GetMaterialManager()
{
    return *m_material_manager;
}

glTFRenderMeshManager& glTFRenderResourceManager::GetMeshManager()
{
    return *m_mesh_manager;
}

bool glTFRenderResourceManager::ApplyMaterial(IRHIDescriptorHeap& descriptor_heap, glTFUniqueID material_ID, unsigned slot_index, bool isGraphicsPipeline)
{
    return m_material_manager->ApplyMaterialRenderResource(*this, descriptor_heap, material_ID, slot_index, isGraphicsPipeline);
}

bool glTFRenderResourceManager::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    const glTFScenePrimitive* primitive = dynamic_cast<const glTFScenePrimitive*>(&object);
    
    if (primitive && primitive->HasMaterial())
    {    
        // Material texture resource descriptor is alloc within current heap
        RETURN_IF_FALSE(resource_manager.GetMaterialManager().InitMaterialRenderResource(resource_manager, primitive->GetMaterial()))
    }
    
    if (!primitive /*|| !primitive->IsVisible()*/)
    {
        return false;
    }
    
    return m_mesh_manager->AddOrUpdatePrimitive(resource_manager, primitive);
}

void glTFRenderResourceManager::SetCurrentPSO(std::shared_ptr<IRHIPipelineStateObject> pso)
{
    if (m_current_pass_pso != pso)
    {
        CloseCommandListAndExecute(false);    
    }
    
    m_current_pass_pso = pso;
}

const glTFRenderResourceFrameManager& glTFRenderResourceManager::GetCurrentFrameResourceManager() const
{
    return GetFrameResourceManagerByIndex(GetCurrentBackBufferIndex());
}

glTFRenderResourceFrameManager& glTFRenderResourceManager::GetCurrentFrameResourceManager()
{
    return GetFrameResourceManagerByIndex(GetCurrentBackBufferIndex());
}

const glTFRenderResourceFrameManager& glTFRenderResourceManager::GetFrameResourceManagerByIndex(unsigned index) const
{
    return m_frame_resource_managers[index];
}

glTFRenderResourceFrameManager& glTFRenderResourceManager::GetFrameResourceManagerByIndex(unsigned index)
{
    return m_frame_resource_managers[index];
}

unsigned glTFRenderResourceManager::GetBackBufferCount()
{
    return backBufferCount;
}

glTFRenderResourceUtils::GBufferSignatureAllocations& glTFRenderResourceManager::GetGBufferAllocations()
{
    return *m_GBuffer_allocations;
}

glTFRadiosityRenderer& glTFRenderResourceManager::GetRadiosityRenderer()
{
    return *m_radiosity_renderer; 
}

const glTFRadiosityRenderer& glTFRenderResourceManager::GetRadiosityRenderer() const
{
    return *m_radiosity_renderer;
}

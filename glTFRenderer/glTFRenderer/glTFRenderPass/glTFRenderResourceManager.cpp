// DX use [0, 1] as depth clip range
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glTFRenderResourceManager.h"

#include "glTFRenderMaterialManager.h"
#include "glTFLight/glTFDirectionalLight.h"
#include "glTFLight/glTFLightBase.h"
#include "glTFRenderSystem/VT/VirtualTextureSystem.h"
#include "glTFRHI/RHIResourceFactoryImpl.hpp"
#include "RenderWindow/glTFWindow.h"
#include "glTFScene/glTFSceneGraph.h"
#include "glTFScene/glTFSceneView.h"

#define EXIT_WHEN_FALSE(x) if (!(x)) {assert(false); return false;}

constexpr size_t backBufferCount = 3;

bool glTFPerFrameRenderResourceData::InitSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device)
{
    GLTF_CHECK(!m_scene_view_buffer);
    if (!m_scene_view_buffer)
    {
        RHIBufferDesc scene_view_buffer_desc;
        scene_view_buffer_desc.name = L"SceneViewConstantBuffer";
        scene_view_buffer_desc.width = 64ull * 1024;
        scene_view_buffer_desc.height = 1;
        scene_view_buffer_desc.depth = 1;
        scene_view_buffer_desc.type = RHIBufferType::Upload;
        scene_view_buffer_desc.resource_type = RHIBufferResourceType::Buffer;
        scene_view_buffer_desc.resource_data_type = RHIDataFormat::UNKNOWN;
        scene_view_buffer_desc.state = RHIResourceStateType::STATE_COMMON;
        scene_view_buffer_desc.usage = RHIResourceUsageFlags::RUF_ALLOW_CBV;
        memory_manager.AllocateBufferMemory(device, scene_view_buffer_desc, m_scene_view_buffer);
    }

    return true;
}

bool glTFPerFrameRenderResourceData::UpdateSceneViewData(IRHIMemoryManager& memory_manager,
                                                         IRHIDevice& device, const ConstantBufferSceneView& scene_view)
{
    m_scene_view = scene_view;
    
    GLTF_CHECK(m_scene_view_buffer);
    return memory_manager.UploadBufferData(*m_scene_view_buffer, &m_scene_view, 0 ,sizeof(m_scene_view));
}

const ConstantBufferSceneView& glTFPerFrameRenderResourceData::GetSceneView() const
{
    return m_scene_view;
}

std::shared_ptr<IRHIBufferAllocation> glTFPerFrameRenderResourceData::GetSceneViewBufferAllocation() const
{
    GLTF_CHECK(m_scene_view_buffer);
    return m_scene_view_buffer;
}

bool glTFPerFrameRenderResourceData::InitShadowmapSceneViewData(IRHIMemoryManager& memory_manager, IRHIDevice& device, unsigned light_id)
{
    GLTF_CHECK(!m_shadowmap_view_buffers[light_id]);
    
    RHIBufferDesc scene_view_buffer_desc;
    scene_view_buffer_desc.name = L"ShadowmapViewConstantBuffer";
    scene_view_buffer_desc.width = 64ull * 1024;
    scene_view_buffer_desc.height = 1;
    scene_view_buffer_desc.depth = 1;
    scene_view_buffer_desc.type = RHIBufferType::Upload;
    scene_view_buffer_desc.resource_type = RHIBufferResourceType::Buffer;
    scene_view_buffer_desc.resource_data_type = RHIDataFormat::UNKNOWN;
    scene_view_buffer_desc.state = RHIResourceStateType::STATE_COMMON;
    scene_view_buffer_desc.usage = RHIResourceUsageFlags::RUF_ALLOW_CBV;
    return memory_manager.AllocateBufferMemory(device, scene_view_buffer_desc, m_shadowmap_view_buffers[light_id]);
}

bool glTFPerFrameRenderResourceData::UpdateShadowmapSceneViewData(IRHIMemoryManager& memory_manager,
                                                                  IRHIDevice& device, unsigned light_id, const ConstantBufferSceneView& scene_view)
{
    m_shadowmap_view_data[light_id] = scene_view;

    GLTF_CHECK(m_shadowmap_view_buffers[light_id]);
    return memory_manager.UploadBufferData(*m_shadowmap_view_buffers[light_id], &m_shadowmap_view_data[light_id], 0 ,sizeof(m_shadowmap_view_data[light_id]));
}

bool glTFPerFrameRenderResourceData::ContainsLightShadowmapViewData(unsigned light_id) const
{
    return m_shadowmap_view_data.contains(light_id); 
}

const ConstantBufferSceneView& glTFPerFrameRenderResourceData::GetShadowmapSceneView(unsigned light_id) const
{
    return m_shadowmap_view_data.at(light_id);
}

const std::map<unsigned, ConstantBufferSceneView>& glTFPerFrameRenderResourceData::GetShadowmapSceneViews() const
{
    return m_shadowmap_view_data;
}

std::shared_ptr<IRHIBufferAllocation> glTFPerFrameRenderResourceData::GetShadowmapViewBufferAllocation(unsigned light_id) const
{
    return m_shadowmap_view_buffers.at(light_id);
}

bool glTFRenderResourceManager::InitResourceManager(unsigned width, unsigned height, HWND handle)
{
    m_material_manager = std::make_shared<glTFRenderMaterialManager>();
    m_mesh_manager = std::make_shared<glTFRenderMeshManager>();
    m_gBuffer_allocations = std::make_shared<glTFRenderResourceUtils::GBufferSignatureAllocations>();
    
    m_factory = RHIResourceFactory::CreateRHIResource<IRHIFactory>();
    EXIT_WHEN_FALSE(m_factory->InitFactory())  
    
    m_device = RHIResourceFactory::CreateRHIResource<IRHIDevice>();
    EXIT_WHEN_FALSE(m_device->InitDevice(*m_factory))
    
    m_command_queue = RHIResourceFactory::CreateRHIResource<IRHICommandQueue>();
    EXIT_WHEN_FALSE(m_command_queue->InitCommandQueue(*m_device))
    
    m_swap_chain = RHIResourceFactory::CreateRHIResource<IRHISwapChain>();
    RHITextureDesc swap_chain_texture_desc("swap_chain_back_buffer",
        width, height,
        RHIDataFormat::R8G8B8A8_UNORM,
        static_cast<RHIResourceUsageFlags>(RUF_ALLOW_RENDER_TARGET | RUF_TRANSFER_DST),
        {
            .clear_format = RHIDataFormat::R8G8B8A8_UNORM,
            .clear_color = {0.0f, 0.0f, 0.0f, 0.0f}
        });

    RHISwapChainDesc swap_chain_desc;
    swap_chain_desc.hwnd = handle;
    swap_chain_desc.chain_mode = VSYNC;
    swap_chain_desc.full_screen = false;
    EXIT_WHEN_FALSE(m_swap_chain->InitSwapChain(*m_factory, *m_device, *m_command_queue, swap_chain_texture_desc, swap_chain_desc ))

    EXIT_WHEN_FALSE(InitMemoryManager())
    
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

    RHITextureClearValue clear_value;
    clear_value.clear_format = RHIDataFormat::R8G8B8A8_UNORM_SRGB;
    clear_value.clear_color = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};

    m_swapchain_RTs = m_render_target_manager->CreateRenderTargetFromSwapChain(*m_device, *this, *m_swap_chain, clear_value);

    for (unsigned i = 0; i < GetBackBufferCount(); ++i)
    {
        auto depth_texture = m_render_target_manager->CreateRenderTarget(
            *m_device, *this,
            RHITextureDesc::MakeDepthTextureDesc(*this), {
                .type = RHIRenderTargetType::DSV,
                .format = RHIDataFormat::D32_FLOAT
            });
    
        m_export_texture_map[RenderPassResourceTableId::Depth].push_back(depth_texture->m_source);
        m_export_texture_descriptor_map[RenderPassResourceTableId::Depth].push_back(depth_texture);
    }
    
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
	            
                else if ((primitive->GetVertexLayout() != resolved_vertex_layout))
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

    m_per_frame_render_resource_data.resize(GetBackBufferCount());
    auto directional_lights = scene_graph.GetAllTypedNodes<glTFDirectionalLight>();
    for (auto& frame_resource : m_per_frame_render_resource_data)
    {
        frame_resource.InitSceneViewData(GetMemoryManager(), GetDevice());
        for (auto& directional_light : directional_lights)
        {
            frame_resource.InitShadowmapSceneViewData(GetMemoryManager(), GetDevice(), directional_light->GetID());
        }
    }
    
    return true;
}

bool glTFRenderResourceManager::InitMemoryManager()
{
    m_memory_manager = RHIResourceFactory::CreateRHIResource<IRHIMemoryManager>();
    EXIT_WHEN_FALSE(m_memory_manager->InitMemoryManager(*m_device, *m_factory,
            {
            256,
            64,
            64
            }))
    return true;
}

bool glTFRenderResourceManager::InitRenderSystems()
{
    for (auto& render_system : m_render_systems)
    {
        render_system->InitRenderSystem(*this);
    }
    return true;
}

IRHIFactory& glTFRenderResourceManager::GetFactory() const
{
    return *m_factory;
}

IRHIDevice& glTFRenderResourceManager::GetDevice() const
{
    return *m_device;
}

IRHISwapChain& glTFRenderResourceManager::GetSwapChain() const
{
    return *m_swap_chain;
}

IRHICommandQueue& glTFRenderResourceManager::GetCommandQueue() const
{
    return *m_command_queue;
}

bool glTFRenderResourceManager::IsMemoryManagerValid() const
{
    return !!m_memory_manager;
}

IRHIMemoryManager& glTFRenderResourceManager::GetMemoryManager() const
{
    GLTF_CHECK(IsMemoryManagerValid());
    return *m_memory_manager;
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

void glTFRenderResourceManager::CloseCurrentCommandListAndExecute(const RHIExecuteCommandListContext& context, bool wait)
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % backBufferCount;
    if (!m_command_list_record_state[current_frame_index])
    {
        return;
    }
    
    auto& command_list = *m_command_lists[current_frame_index];
    
    const bool closed = RHIUtils::Instance().CloseCommandList(command_list);
    GLTF_CHECK(closed);

    GLTF_CHECK(RHIUtils::Instance().ExecuteCommandList(command_list, GetCommandQueue(), context));
    if (wait)
    {
        RHIUtils::Instance().WaitCommandListFinish(command_list);
    }
    
    m_command_list_record_state[current_frame_index] = false;
}

void glTFRenderResourceManager::WaitPresentFinished()
{
    if (m_swap_chain)
    {
        m_swap_chain->HostWaitPresentFinished(GetDevice());    
    }
}

void glTFRenderResourceManager::WaitLastFrameFinish() const
{
    const auto current_frame_index = GetCurrentBackBufferIndex() % backBufferCount;
    RHIUtils::Instance().WaitCommandListFinish(*m_command_lists[current_frame_index]);
}

void glTFRenderResourceManager::WaitAllFrameFinish() const
{
    for (const auto& command_list : m_command_lists)
    {
        RHIUtils::Instance().WaitCommandListFinish(*command_list);
    }
    
    for (auto& render_system : m_render_systems)
    {
        render_system->ShutdownRenderSystem();
    }

    // Wait queue
    RHIUtils::Instance().WaitCommandQueueIdle(*m_command_queue);
}

void glTFRenderResourceManager::ResetCommandAllocator()
{
    CloseCurrentCommandListAndExecute({}, true);
    RHIUtils::Instance().ResetCommandAllocator(GetCurrentFrameCommandAllocator());
}

void glTFRenderResourceManager::WaitAndClean()
{
    if (IsMemoryManagerValid())
    {
        GetMemoryManager().ReleaseAllResource(*this);    
    }
}

IRHIRenderTargetManager& glTFRenderResourceManager::GetRenderTargetManager()
{
    return *m_render_target_manager;
}

IRHICommandAllocator& glTFRenderResourceManager::GetCurrentFrameCommandAllocator()
{
    return *m_command_allocators[GetCurrentBackBufferIndex() % backBufferCount];
}

IRHITexture& glTFRenderResourceManager::GetCurrentFrameSwapChainTexture()
{
    return *m_swapchain_RTs[GetCurrentBackBufferIndex() % backBufferCount]->m_source;
}

IRHITexture& glTFRenderResourceManager::GetDepthTextureRef()
{
    return *GetDepthTexture();
}

std::shared_ptr<IRHITexture> glTFRenderResourceManager::GetDepthTexture()
{
    return m_export_texture_map[RenderPassResourceTableId::Depth][GetCurrentBackBufferIndex()];
}

IRHITextureDescriptorAllocation& glTFRenderResourceManager::GetCurrentFrameSwapChainRTV()
{
    return *m_swapchain_RTs[GetCurrentBackBufferIndex() % backBufferCount];
}

IRHITextureDescriptorAllocation& glTFRenderResourceManager::GetDepthDSV()
{
    return *m_export_texture_descriptor_map[RenderPassResourceTableId::Depth][GetCurrentBackBufferIndex()];
}

unsigned glTFRenderResourceManager::GetCurrentBackBufferIndex() const
{
    return m_swap_chain->GetCurrentBackBufferIndex();
}

glTFRenderMaterialManager& glTFRenderResourceManager::GetMaterialManager()
{
    return *m_material_manager;
}

glTFRenderMeshManager& glTFRenderResourceManager::GetMeshManager()
{
    return *m_mesh_manager;
}

bool glTFRenderResourceManager::TryProcessSceneObject(glTFRenderResourceManager& resource_manager,
    const glTFSceneObjectBase& object)
{
    const glTFScenePrimitive* primitive = dynamic_cast<const glTFScenePrimitive*>(&object);
    
    if (primitive && primitive->HasMaterial())
    {    
        // Material texture resource descriptor is alloc within current heap
        RETURN_IF_FALSE(resource_manager.GetMaterialManager().AddMaterialRenderResource(resource_manager, primitive->GetMaterial()))
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
        CloseCurrentCommandListAndExecute({}, false);    
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
    return *m_gBuffer_allocations;
}

bool glTFRenderResourceManager::ExportResourceTexture(const RHITextureDesc& desc, RenderPassResourceTableId entry_id,
    std::vector<std::shared_ptr<IRHITexture>>& out_texture)
{
    for (unsigned i = 0; i < GetBackBufferCount(); ++i)
    {
        // Export texture resource must create it first, add table id to internal tracked map
        std::shared_ptr<IRHITextureAllocation> out_texture_allocation;
        bool created = GetMemoryManager().AllocateTextureMemory(GetDevice(), *this, desc, out_texture_allocation);
        GLTF_CHECK(created);

        out_texture.push_back(out_texture_allocation->m_texture);
        m_export_texture_map[entry_id].push_back(out_texture_allocation->m_texture);
        m_export_texture_allocation_map[entry_id].push_back(out_texture_allocation);
    }
    
    return true;
}

bool glTFRenderResourceManager::ImportResourceTexture(const RHITextureDesc& desc, RenderPassResourceTableId entry_id,
    std::vector<std::shared_ptr<IRHITexture>>& out_texture_allocation)
{
    // Import texture resource need query table id from tracked map and return allocation
    auto find_texture = m_export_texture_map.find(entry_id);
    GLTF_CHECK(find_texture != m_export_texture_map.end());

    out_texture_allocation = find_texture->second;
    return true;
}

std::vector<glTFPerFrameRenderResourceData>& glTFRenderResourceManager::GetPerFrameRenderResourceData()
{
    return m_per_frame_render_resource_data;
}

const std::vector<glTFPerFrameRenderResourceData>& glTFRenderResourceManager::GetPerFrameRenderResourceData() const
{
    return m_per_frame_render_resource_data;
}

RenderGraphNodeUtil::RenderGraphNodeFinalOutput& glTFRenderResourceManager::GetFinalOutput()
{
    return m_final_output;
}

void glTFRenderResourceManager::AddRenderSystem(std::shared_ptr<RenderSystemBase> render_system)
{
    m_render_systems.push_back(render_system);
}

std::vector<std::shared_ptr<RenderSystemBase>> glTFRenderResourceManager::GetRenderSystems() const
{
    return m_render_systems;
}

void glTFRenderResourceManager::TickFrame()
{
    m_memory_manager->TickFrame();
}

void glTFRenderResourceManager::TickSceneUpdating(const glTFSceneView& scene_view,
                                                  glTFRenderResourceManager& resource_manager, const glTFSceneGraph& scene_graph, size_t delta_time_ms)
{
    // Update scene view
    auto scene_view_constant_buffer = scene_view.CreateSceneViewConstantBuffer();
    auto& upload_scene_view_buffer= resource_manager.GetPerFrameRenderResourceData()[resource_manager.GetCurrentBackBufferIndex()];
    upload_scene_view_buffer.UpdateSceneViewData(resource_manager.GetMemoryManager(), resource_manager.GetDevice(), scene_view_constant_buffer);

    std::vector<const glTFSceneNode*> dirty_objects = scene_view.GetDirtySceneNodes();
    for (const auto* node : dirty_objects)
    {
        for (const auto& scene_object : node->m_objects)
        {
            resource_manager.TryProcessSceneObject(resource_manager, *scene_object);
        }

        node->ResetDirty();
    }

    resource_manager.GetMaterialManager().Tick(resource_manager);

    // Shadowmap view data upload
    std::vector<const glTFLightBase*> lights;
    for (const auto* node : dirty_objects)
    {
        for (const auto& scene_object : node->m_objects)
        {
            if (const glTFLightBase* light_node = dynamic_cast<const glTFLightBase*>(scene_object.get()))
            {
                lights.push_back(light_node);
            }    
        }
    }

    m_scene_bounds = scene_graph.GetBounds();
    for (const auto& light : lights)
    {
        const auto& direction_light = dynamic_cast<const glTFDirectionalLight*>(light);
        if (!direction_light)
        {
            continue;
        }

        auto light_shadowmap_view = direction_light->GetShadowmapViewInfo(m_scene_bounds, {0.0f, 0.0f, 1.0f, 1.0f});
        
        ConstantBufferSceneView shadowmap_view{};
        shadowmap_view.view_position = light_shadowmap_view.position;
        shadowmap_view.view_matrix = light_shadowmap_view.view_matrix;
        shadowmap_view.inverse_view_matrix = glm::inverse(shadowmap_view.view_matrix);
        shadowmap_view.projection_matrix = light_shadowmap_view.projection_matrix;
        shadowmap_view.inverse_projection_matrix = glm::inverse(shadowmap_view.projection_matrix);
        
        resource_manager.GetPerFrameRenderResourceData()[resource_manager.GetCurrentBackBufferIndex()].UpdateShadowmapSceneViewData(
            resource_manager.GetMemoryManager(), resource_manager.GetDevice(), direction_light->GetID(), shadowmap_view);

        for (int i = 0; i < resource_manager.GetBackBufferCount(); ++i)
        {
            if (!resource_manager.GetPerFrameRenderResourceData()[i].ContainsLightShadowmapViewData(direction_light->GetID()))
            {
                resource_manager.GetPerFrameRenderResourceData()[i].UpdateShadowmapSceneViewData(
                resource_manager.GetMemoryManager(), resource_manager.GetDevice(), direction_light->GetID(), shadowmap_view);
            }
        }
    }
}

glTF_AABB::AABB glTFRenderResourceManager::GetSceneBounds() const
{
    return m_scene_bounds;
}

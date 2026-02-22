#include "RendererInterface.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "InternalResourceHandleTable.h"
#include "RendererSceneCommon.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "RHIConfigSingleton.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"
#include "RendererSceneGraph.h"
#include "RenderWindow/glTFWindow.h"
#include "RHIInterface/IRHIDescriptorManager.h"
#include "RHIInterface/IRHIDescriptorUpdater.h"
#include "RHIInterface/IRHIMemoryManager.h"
#include "RHIInterface/IRHISwapChain.h"
#include "RHIInterface/RHIIndexBuffer.h"
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>

namespace RendererInterface
{
    struct RenderGraph::GPUProfilerState
    {
        struct FrameSlot
        {
            bool has_pending_frame_stats{false};
            unsigned query_count{0};
            FrameStats pending_frame_stats{};
        };

        bool supported{false};
        unsigned max_pass_count{512};
        unsigned max_query_count{1024};
        double ticks_per_second{0.0};
        std::vector<FrameSlot> frame_slots{};
    };

    namespace
    {
        enum class ResourceKind
        {
            Buffer,
            Texture,
            RenderTarget,
        };

        struct ResourceKey
        {
            ResourceKind kind;
            unsigned value;

            bool operator<(const ResourceKey& other) const
            {
                if (kind != other.kind)
                {
                    return kind < other.kind;
                }
                return value < other.value;
            }
        };

        struct ResourceAccessSet
        {
            std::set<ResourceKey> reads;
            std::set<ResourceKey> writes;
        };

        const char* ToResourceKindName(ResourceKind kind)
        {
            switch (kind)
            {
            case ResourceKind::Buffer:
                return "Buffer";
            case ResourceKind::Texture:
                return "Texture";
            case ResourceKind::RenderTarget:
                return "RenderTarget";
            }
            return "Unknown";
        }

        const char* ToRenderPassTypeName(RenderPassType type)
        {
            switch (type)
            {
            case RenderPassType::GRAPHICS:
                return "Graphics";
            case RenderPassType::COMPUTE:
                return "Compute";
            case RenderPassType::RAY_TRACING:
                return "RayTracing";
            }
            return "Unknown";
        }

        bool HasDependency(const RenderGraphNodeDesc& desc, RenderGraphNodeHandle handle)
        {
            return std::find(desc.dependency_render_graph_nodes.begin(),
                desc.dependency_render_graph_nodes.end(),
                handle) != desc.dependency_render_graph_nodes.end();
        }

        void HashCombine(std::size_t& seed, std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        ResourceAccessSet CollectResourceAccess(const RenderGraphNodeDesc& desc)
        {
            ResourceAccessSet access;

            for (const auto& buffer_pair : desc.draw_info.buffer_resources)
            {
                const auto& binding = buffer_pair.second;
                ResourceKey key{ResourceKind::Buffer, binding.buffer_handle.value};

                switch (binding.binding_type)
                {
                case BufferBindingDesc::CBV:
                case BufferBindingDesc::SRV:
                    access.reads.insert(key);
                    break;
                case BufferBindingDesc::UAV:
                    access.reads.insert(key);
                    access.writes.insert(key);
                    break;
                }
            }

            for (const auto& texture_pair : desc.draw_info.texture_resources)
            {
                const auto& binding = texture_pair.second;
                for (const auto texture_handle : binding.textures)
                {
                    ResourceKey key{ResourceKind::Texture, texture_handle.value};
                    if (binding.type == TextureBindingDesc::SRV)
                    {
                        access.reads.insert(key);
                    }
                    else
                    {
                        access.reads.insert(key);
                        access.writes.insert(key);
                    }
                }
            }

            for (const auto& render_target_pair : desc.draw_info.render_target_texture_resources)
            {
                const auto& binding = render_target_pair.second;
                for (const auto render_target_handle : binding.render_target_texture)
                {
                    ResourceKey key{ResourceKind::RenderTarget, render_target_handle.value};
                    if (binding.type == RenderTargetTextureBindingDesc::SRV)
                    {
                        access.reads.insert(key);
                    }
                    else
                    {
                        access.reads.insert(key);
                        access.writes.insert(key);
                    }
                }
            }

            for (const auto& render_target_pair : desc.draw_info.render_target_resources)
            {
                const auto& binding = render_target_pair.second;
                ResourceKey key{ResourceKind::RenderTarget, render_target_pair.first.value};
                access.writes.insert(key);
                if (binding.load_op == RenderPassAttachmentLoadOp::LOAD)
                {
                    access.reads.insert(key);
                }
            }

            return access;
        }

        bool IsSameBufferBindingDesc(const BufferBindingDesc& lhs, const BufferBindingDesc& rhs)
        {
            return lhs.buffer_handle == rhs.buffer_handle &&
                lhs.binding_type == rhs.binding_type &&
                lhs.stride == rhs.stride &&
                lhs.count == rhs.count &&
                lhs.is_structured_buffer == rhs.is_structured_buffer &&
                lhs.use_count_buffer == rhs.use_count_buffer &&
                lhs.count_buffer_offset == rhs.count_buffer_offset;
        }

        bool IsSameTextureBindingDesc(const TextureBindingDesc& lhs, const TextureBindingDesc& rhs)
        {
            return lhs.type == rhs.type && lhs.textures == rhs.textures;
        }

        bool IsSameRenderTargetTextureBindingDesc(const RenderTargetTextureBindingDesc& lhs, const RenderTargetTextureBindingDesc& rhs)
        {
            return lhs.type == rhs.type &&
                lhs.name == rhs.name &&
                lhs.render_target_texture == rhs.render_target_texture;
        }

        RHIPrimitiveTopologyType ConvertToRHIPrimitiveTopology(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::TRIANGLE_LIST:
                return RHIPrimitiveTopologyType::TRIANGLELIST;
            }

            GLTF_CHECK(false);
            return RHIPrimitiveTopologyType::TRIANGLELIST;
        }
    }

    RenderWindow::RenderWindow(const RenderWindowDesc& desc)
        : m_desc(desc)
        , m_handle(NULL_HANDLE)
        , m_hwnd(nullptr)
    {
        glTFWindow::Get().SetWidth(desc.width);
        glTFWindow::Get().SetHeight(desc.height);
        
        glTFWindow::Get().InitAndShowWindow();
        m_handle = InternalResourceHandleTable::Instance().RegisterWindow(*this);
        m_hwnd = glTFWindow::Get().GetHWND();
        
        m_input_device = std::make_shared<RendererInputDevice>();
        glTFWindow::Get().SetInputManager(m_input_device);

        // Register window callback
        GLTF_CHECK(glTFWindow::Get().RegisterCallbackEventNative());
    }

    RenderWindowHandle RenderWindow::GetHandle() const
    {
        return m_handle;
    }

    unsigned RenderWindow::GetWidth() const
    {
        return static_cast<unsigned>(glTFWindow::Get().GetWidth());
    }

    unsigned RenderWindow::GetHeight() const
    {
        return static_cast<unsigned>(glTFWindow::Get().GetHeight());
    }

    HWND RenderWindow::GetHWND() const
    {
        return m_hwnd;
    }

    void RenderWindow::EnterWindowEventLoop()
    {
        glTFWindow::Get().UpdateWindow();
    }

    void RenderWindow::RegisterTickCallback(const RenderWindowTickCallback& callback)
    {
        glTFWindow::Get().SetTickCallback(callback);
    }

    const RendererInputDevice& RenderWindow::GetInputDeviceConst() const
    {
        return *m_input_device;
    }

    RendererInputDevice& RenderWindow::GetInputDevice()
    {
        return *m_input_device;
    }

    RendererSceneResourceManager::RendererSceneResourceManager(ResourceOperator& allocator,const RenderSceneDesc& desc)
        : m_allocator(allocator)
    {
        std::shared_ptr<RendererSceneGraph> scene_graph = std::make_shared<RendererSceneGraph>();
        m_render_scene_handle = InternalResourceHandleTable::Instance().RegisterRenderScene(scene_graph);
        
        glTFLoader loader;
        bool loaded = loader.LoadFile(desc.scene_file_name);
        GLTF_CHECK(loaded);
        
        bool added = scene_graph->InitializeRootNodeWithSceneFile_glTF(loader);
        GLTF_CHECK(added);

        struct SceneMeshDataOffsetInfo
        {
            unsigned start_face_index;
            unsigned material_index;
            unsigned start_vertex_index;
        };

        struct SceneMeshFaceInfo
        {
            unsigned vertex_index[3];
        };

        struct SceneMeshVertexInfo
        {
            float position[4];
            float normal[4];
            float tangent[4];
            float uv[4];
        };

        struct glTFMeshInstanceRenderResource
        {
            glm::mat4 m_instance_transform;
            unsigned m_mesh_render_resource;
            unsigned m_instance_material_id;
            bool m_normal_mapping;
        };

        std::vector<SceneMeshDataOffsetInfo> start_offset_infos;
        std::vector<SceneMeshVertexInfo> mesh_vertex_infos;
        std::vector<SceneMeshFaceInfo> mesh_face_vertex_infos;
        unsigned start_face_offset = 0;

        const auto& meshes = scene_graph->GetMeshes();
        for (const auto& mesh_pair : meshes)
        {
            const auto& mesh = *mesh_pair.second;

            const unsigned vertex_offset = static_cast<unsigned>(mesh_vertex_infos.size());

            SceneMeshDataOffsetInfo mesh_info =
            {
                .start_face_index = start_face_offset,
                // TODO: Material system 
                .material_index = 0,
                .start_vertex_index = vertex_offset,
            };
                
            start_offset_infos.push_back(mesh_info);
            const size_t face_count = mesh.GetIndexBuffer().index_count / 3;
            const auto& index_buffer = mesh.GetIndexBuffer();
            for (size_t i = 0; i < face_count; ++i)
            {
                mesh_face_vertex_infos.push_back(
                {
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i),
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i + 1),
                    vertex_offset + index_buffer.GetIndexByOffset(3 * i + 2),
                });
            }

            for (size_t i = 0; i < mesh.GetVertexBuffer().vertex_count; ++i)
            {
                SceneMeshVertexInfo vertex_info;
                size_t out_data_size = 0;
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_POSITION, i, &vertex_info.position, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_NORMAL, i, &vertex_info.normal, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TANGENT, i, &vertex_info.tangent, out_data_size);
                mesh.GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TEXCOORD0, i, &vertex_info.uv, out_data_size);
                mesh_vertex_infos.push_back(vertex_info);
            }
        
            start_face_offset += face_count;
        }
    }

    bool RendererSceneResourceManager::AccessSceneData(RendererSceneMeshDataAccessorBase& data_accessor)
    {
        const auto& scene_graph = InternalResourceHandleTable::Instance().GetRenderScene(m_render_scene_handle);
        auto scene_node_traverse = [&](RendererSceneNode& node)
        {
            if (node.HasMesh())
            {
                auto absolute_transform = node.GetAbsoluteTransform();
                
                for (const auto& mesh : node.GetMeshes())
                {
                    auto mesh_id = mesh->GetID();
                    
                    if (!data_accessor.HasMeshData(mesh_id))
                    {
                        auto vertex_count = mesh->GetVertexBuffer().vertex_count;
                        std::vector<float> vertex_positions(vertex_count * 3);
                        std::vector<float> vertex_normals(vertex_count * 3);
                        std::vector<float> vertex_tangents(vertex_count * 4);
                        std::vector<float> vertex_uvs(vertex_count * 2);

                        for (size_t i = 0; i < vertex_count; ++i)
                        {
                            size_t out_data_size = 0;
                            mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_POSITION, i, &vertex_positions[3 * i], out_data_size);
                            GLTF_CHECK(out_data_size == 3 * sizeof(float));
                        
                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_NORMAL, i, &vertex_normals[3 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 3 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no normal vertex data!\n", mesh->GetID());
                            }

                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TANGENT, i, &vertex_tangents[4 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 4 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no tangent vertex data!\n", mesh->GetID());
                            }
                            
                            if (mesh->GetVertexBuffer().GetVertexAttributeDataByIndex(VertexAttributeType::VERTEX_TEXCOORD0, i, &vertex_uvs[2 * i], out_data_size))
                            {
                                GLTF_CHECK(out_data_size == 2 * sizeof(float));    
                            }
                            else
                            {
                                LOG_FORMAT_FLUSH("[WARN] Mesh %d has no uv vertex data!\n", mesh->GetID());
                            }
                        }

                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_POSITION_FLOAT3, mesh_id, vertex_positions.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_NORMAL_FLOAT3, mesh_id, vertex_normals.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_TANGENT_FLOAT4, mesh_id, vertex_tangents.data(), vertex_count);
                        data_accessor.AccessMeshData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::VERTEX_TEXCOORD0_FLOAT2, mesh_id, vertex_uvs.data(), vertex_count);

                        auto index = mesh->GetIndexBuffer().data.get();
                        auto index_count = mesh->GetIndexBuffer().index_count;
                        data_accessor.AccessMeshData(mesh->GetIndexBuffer().format == RHIDataFormat::R16_UINT ? RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INDEX_HALF : RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INDEX_INT, mesh_id, index, index_count);

                        data_accessor.AccessMaterialData(mesh->GetMaterial(), mesh_id);
                    }
                    
                    data_accessor.AccessInstanceData(RendererSceneMeshDataAccessorBase::MeshDataAccessorType::INSTANCE_MAT4x4, node.GetID(), mesh_id, &absolute_transform, 1);
                }
            }
            
            // No stop
            return false;
        };
        
        scene_graph->GetRootNode().Traverse(scene_node_traverse);
        
        return true;
    }

    RendererSceneAABB RendererSceneResourceManager::GetSceneBounds() const
    {
        const auto& scene_graph = InternalResourceHandleTable::Instance().GetRenderScene(m_render_scene_handle);
        GLTF_CHECK(scene_graph);
        return scene_graph->GetBounds();
    }

    ResourceOperator::ResourceOperator(RenderDeviceDesc device)
    {
        if (!m_resource_manager)
        {
            RHIConfigSingleton::Instance().SetGraphicsAPIType(device.type == VULKAN ?
            RHIGraphicsAPIType::RHI_GRAPHICS_API_Vulkan : RHIGraphicsAPIType::RHI_GRAPHICS_API_DX12);
    
            RHIConfigSingleton::Instance().InitGraphicsAPI();
            
            m_resource_manager = std::make_shared<ResourceManager>();
            m_resource_manager->InitResourceManager(device);
        }
    }

    ShaderHandle ResourceOperator::CreateShader(const ShaderDesc& desc)
    {
        return m_resource_manager->CreateShader(desc);
    }

    TextureHandle ResourceOperator::CreateTexture(const TextureDesc& desc)
    {
        const RHIDataFormat rhi_format = RendererInterfaceRHIConverter::ConvertToRHIFormat(desc.format);
        GLTF_CHECK(rhi_format != RHIDataFormat::UNKNOWN);

        RHITextureClearValue clear_value{};
        clear_value.clear_format = rhi_format;
        clear_value.clear_color[0] = 0.0f;
        clear_value.clear_color[1] = 0.0f;
        clear_value.clear_color[2] = 0.0f;
        clear_value.clear_color[3] = 0.0f;

        RHITextureDesc texture_desc(
            "runtime_texture",
            desc.width,
            desc.height,
            rhi_format,
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST | (desc.generate_mipmaps ? RUF_CONTAINS_MIPMAP : RUF_NONE)),
            clear_value);

        std::shared_ptr<IRHITextureAllocation> texture_allocation = nullptr;
        bool created = false;
        if (!desc.data.empty())
        {
            const bool has_data = texture_desc.SetTextureData(desc.data.data(), desc.data.size());
            GLTF_CHECK(has_data);
            created = m_resource_manager->GetMemoryManager().AllocateTextureMemoryAndUpload(
                m_resource_manager->GetDevice(),
                m_resource_manager->GetCommandListForRecordPassCommand(),
                m_resource_manager->GetMemoryManager(),
                texture_desc,
                texture_allocation);
        }
        else
        {
            created = m_resource_manager->GetMemoryManager().AllocateTextureMemory(
                m_resource_manager->GetDevice(),
                texture_desc,
                texture_allocation);
        }
        GLTF_CHECK(created);

        texture_allocation->m_texture->Transition(
            m_resource_manager->GetCommandListForRecordPassCommand(),
            RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);

        return InternalResourceHandleTable::Instance().RegisterTexture(texture_allocation);
    }

    TextureHandle ResourceOperator::CreateTexture(const TextureFileDesc& desc)
    {
        const std::wstring convert_path = to_wide_string(desc.uri);
        ImageLoadResult result;
        if (!glTFImageIOUtil::Instance().LoadImageByFilename(convert_path.c_str(), result))
        {
            GLTF_CHECK(false);
            return NULL_HANDLE;
        }
        RHITextureDesc texture_desc{};
        texture_desc.InitWithLoadedData(result);

        std::shared_ptr<IRHITextureAllocation> texture_allocation = nullptr;
        bool created = m_resource_manager->GetMemoryManager().AllocateTextureMemoryAndUpload(m_resource_manager->GetDevice(), m_resource_manager->GetCommandListForRecordPassCommand(), m_resource_manager->GetMemoryManager(), texture_desc, texture_allocation);
        GLTF_CHECK(created);

        texture_allocation->m_texture->Transition(m_resource_manager->GetCommandListForRecordPassCommand(), RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);

        return InternalResourceHandleTable::Instance().RegisterTexture(texture_allocation);
    }

    BufferHandle ResourceOperator::CreateBuffer(const BufferDesc& desc)
    {
        return m_resource_manager->CreateBuffer(desc);
    }

    IndexedBufferHandle ResourceOperator::CreateIndexedBuffer(const BufferDesc& desc)
    {
        return m_resource_manager->CreateIndexedBuffer(desc);   
    }

    RenderTargetHandle ResourceOperator::CreateRenderTarget(const RenderTargetDesc& desc)
    {
        return m_resource_manager->CreateRenderTarget(desc);
    }

    RenderTargetHandle ResourceOperator::CreateRenderTarget(const std::string& name, unsigned width, unsigned height,
        PixelFormat format, RenderTargetClearValue clear_value, ResourceUsage usage)
    {
        RendererInterface::RenderTargetDesc render_target_desc{};
        render_target_desc.name = name;
        render_target_desc.width = width;
        render_target_desc.height = height;
        render_target_desc.format = format;
        render_target_desc.clear = clear_value;
        render_target_desc.usage = usage; 
        return  m_resource_manager->CreateRenderTarget(render_target_desc);
    }

    RenderPassHandle ResourceOperator::CreateRenderPass(const RenderPassDesc& desc)
    {
        std::shared_ptr<RenderPass> render_pass = std::make_shared<RenderPass>(desc);
        render_pass->InitRenderPass(*m_resource_manager);
        
        return InternalResourceHandleTable::Instance().RegisterRenderPass(render_pass);
    }

    RenderSceneHandle ResourceOperator::CreateRenderScene(const RenderSceneDesc& desc)
    {
        std::shared_ptr<RendererSceneGraph> scene_graph = std::make_shared<RendererSceneGraph>();
        
        glTFLoader loader;
        bool loaded = loader.LoadFile(desc.scene_file_name);
        GLTF_CHECK(loaded);
        
        bool added = scene_graph->InitializeRootNodeWithSceneFile_glTF(loader);
        GLTF_CHECK(added);

        const auto traverse_function = [](const RendererSceneNode& node)
        {
            return true;
        };
    
        scene_graph->GetRootNode().Traverse(traverse_function);
        
        return InternalResourceHandleTable::Instance().RegisterRenderScene(scene_graph);
    }

    IRHIDevice& ResourceOperator::GetDevice() const
    {
        return m_resource_manager->GetDevice();
    }

    unsigned ResourceOperator::GetCurrentBackBufferIndex() const
    {
        return m_resource_manager->GetCurrentBackBufferIndex();
    }

    IRHITextureDescriptorAllocation& ResourceOperator::GetCurrentSwapchainRT() const
    {
        return m_resource_manager->GetCurrentSwapchainRT();
    }

    void ResourceOperator::UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc)
    {
        auto buffer = InternalResourceHandleTable::Instance().GetBuffer(handle);
        m_resource_manager->GetMemoryManager().UploadBufferData(m_resource_manager->GetDevice(), m_resource_manager->GetCommandListForRecordPassCommand(), *buffer, upload_desc.data, 0, upload_desc.size);
    }

    void ResourceOperator::WaitFrameRenderFinished()
    {
        return m_resource_manager->WaitFrameRenderFinished();
    }

    void ResourceOperator::InvalidateSwapchainResizeRequest()
    {
        return m_resource_manager->InvalidateSwapchainResizeRequest();
    }

    bool ResourceOperator::ResizeSwapchainIfNeeded(unsigned width, unsigned height)
    {
        return m_resource_manager->ResizeSwapchainIfNeeded(width, height);
    }

    bool ResourceOperator::ResizeWindowDependentRenderTargets(unsigned width, unsigned height)
    {
        return m_resource_manager->ResizeWindowDependentRenderTargets(width, height);
    }

    IRHICommandList& ResourceOperator::GetCommandListForRecordPassCommand(RenderPassHandle pass) const
    {
        return m_resource_manager->GetCommandListForRecordPassCommand(pass);
    }

    IRHIDescriptorManager& ResourceOperator::GetDescriptorManager() const
    {
        return m_resource_manager->GetMemoryManager().GetDescriptorManager();
    }

    IRHIMemoryManager& ResourceOperator::GetMemoryManager() const
    {
        return m_resource_manager->GetMemoryManager();
    }

    IRHICommandQueue& ResourceOperator::GetCommandQueue() const
    {
        return m_resource_manager->GetCommandQueue();
    }

    IRHISwapChain& ResourceOperator::GetCurrentSwapchain() const
    {
        return m_resource_manager->GetSwapChain();
    }

    unsigned ResourceOperator::GetCurrentRenderWidth() const
    {
        return m_resource_manager->GetSwapChain().GetWidth();
    }

    unsigned ResourceOperator::GetCurrentRenderHeight() const
    {
        return m_resource_manager->GetSwapChain().GetHeight();
    }

    bool ResourceOperator::CleanupAllResources(bool clear_window_handles)
    {
        if (!m_resource_manager)
        {
            return true;
        }

        m_resource_manager->WaitFrameRenderFinished();
        m_resource_manager->WaitGPUIdle();

        auto& memory_manager = m_resource_manager->GetMemoryManager();
        const bool cleaned_resources = RHIResourceFactory::CleanupResources(memory_manager);
        GLTF_CHECK(cleaned_resources);
        const bool released_allocations = memory_manager.ReleaseAllResource();
        GLTF_CHECK(released_allocations);

        if (clear_window_handles)
        {
            InternalResourceHandleTable::Instance().ClearAll();
        }
        else
        {
            InternalResourceHandleTable::Instance().ClearRuntimeResources();
        }

        m_resource_manager.reset();
        m_render_passes.clear();

        return cleaned_resources && released_allocations;
    }

    RenderGraph::RenderGraph(ResourceOperator& allocator, RenderWindow& window)
        : m_resource_allocator(allocator)
        , m_window(window)
    {
        GLTF_CHECK(InitDebugUI());
        GLTF_CHECK(InitGPUProfiler());
    }

    RenderGraph::~RenderGraph()
    {
        ShutdownGPUProfiler();
        ShutdownDebugUI();
    }

    RenderGraphNodeHandle RenderGraph::CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc)
    {
        RenderGraphNodeHandle result{static_cast<unsigned>(m_render_graph_nodes.size())};
        m_render_graph_nodes.push_back(render_graph_node_desc);
        return result;
    }

    RenderGraphNodeHandle RenderGraph::CreateRenderGraphNode(ResourceOperator& allocator,const RenderPassSetupInfo& setup_info)
    {
        RendererInterface::RenderPassDesc render_pass_desc{};
        render_pass_desc.type = setup_info.render_pass_type;
        render_pass_desc.render_state = setup_info.render_state;
    
        for (const auto& shader_info : setup_info.shader_setup_infos)
        {
            RendererInterface::ShaderDesc shader_desc{};
            shader_desc.shader_type = shader_info.shader_type; 
            shader_desc.entry_point = shader_info.entry_function;
            shader_desc.shader_file_name = shader_info.shader_file;
            auto shader_handle = allocator.CreateShader(shader_desc);
            
            render_pass_desc.shaders.emplace(shader_info.shader_type, shader_handle);
        }
    
        RendererInterface::RenderPassDrawDesc render_pass_draw_desc{};
        for (const auto& module : setup_info.modules)
        {
            module->BindDrawCommands(render_pass_draw_desc);
        }

        switch (setup_info.render_pass_type)
        {
        case RendererInterface::RenderPassType::GRAPHICS:
            for (const auto& render_target : setup_info.render_targets)
            {
                render_pass_desc.render_target_bindings.push_back(render_target.second);
                render_pass_draw_desc.render_target_resources.emplace(render_target.first, render_target.second);
            }
            break;
        case RendererInterface::RenderPassType::COMPUTE:
            break;
        case RendererInterface::RenderPassType::RAY_TRACING:
            break;
        }
    
        for (const auto& render_target : setup_info.sampled_render_targets)
        {
            render_pass_draw_desc.render_target_texture_resources[render_target.name] = render_target;
        }

        render_pass_draw_desc.texture_resources.insert(setup_info.texture_resources.begin(), setup_info.texture_resources.end());
        render_pass_draw_desc.buffer_resources.insert(setup_info.buffer_resources.begin(), setup_info.buffer_resources.end());
    
        if (setup_info.execute_command.has_value())
        {
            render_pass_draw_desc.execute_commands.push_back(setup_info.execute_command.value());    
        }
        render_pass_draw_desc.execute_commands.insert(
            render_pass_draw_desc.execute_commands.end(),
            setup_info.execute_commands.begin(),
            setup_info.execute_commands.end());

        render_pass_desc.viewport_width = setup_info.viewport_width;
        render_pass_desc.viewport_height = setup_info.viewport_height;
        
        auto render_pass_handle = allocator.CreateRenderPass(render_pass_desc);
    
        RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
        render_graph_node_desc.draw_info = render_pass_draw_desc;
        render_graph_node_desc.render_pass_handle = render_pass_handle;
        render_graph_node_desc.dependency_render_graph_nodes = setup_info.dependency_render_graph_nodes;
        render_graph_node_desc.pre_render_callback = setup_info.pre_render_callback;
        render_graph_node_desc.debug_group = setup_info.debug_group;
        render_graph_node_desc.debug_name = setup_info.debug_name;

        auto render_graph_node_handle = CreateRenderGraphNode(render_graph_node_desc);
        return render_graph_node_handle;
    }

    bool RenderGraph::RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        GLTF_CHECK(!m_render_graph_node_handles.contains(render_graph_node_handle));
        m_render_graph_node_handles.insert(render_graph_node_handle);
        return true;
    }

    bool RenderGraph::RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(m_render_graph_node_handles.contains(render_graph_node_handle));
        m_render_graph_node_handles.erase(render_graph_node_handle);

        auto descriptor_it = m_render_pass_descriptor_resources.find(render_graph_node_handle);
        if (descriptor_it != m_render_pass_descriptor_resources.end())
        {
            ReleaseRenderPassDescriptorResource(descriptor_it->second);
            m_render_pass_descriptor_resources.erase(descriptor_it);
        }
        m_render_pass_descriptor_last_used_frame.erase(render_graph_node_handle);

        m_cached_execution_signature = 0;
        m_cached_execution_order.clear();
        return true;
    }

    bool RenderGraph::UpdateComputeDispatch(RenderGraphNodeHandle render_graph_node_handle, unsigned group_size_x, unsigned group_size_y, unsigned group_size_z)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());

        auto& node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        for (auto& command : node_desc.draw_info.execute_commands)
        {
            if (command.type != ExecuteCommandType::COMPUTE_DISPATCH_COMMAND)
            {
                continue;
            }

            command.parameter.dispatch_parameter.group_size_x = group_size_x;
            command.parameter.dispatch_parameter.group_size_y = group_size_y;
            command.parameter.dispatch_parameter.group_size_z = group_size_z;
            return true;
        }

        return false;
    }

    bool RenderGraph::CompileRenderPassAndExecute()
    {
        m_window.RegisterTickCallback([this](unsigned long long interval)
        {
            if (m_final_color_output_render_target_handle != NULL_HANDLE)
            {
                auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(m_final_color_output_render_target_handle);
                m_final_color_output = render_target->m_source;
            }
            else if (m_final_color_output_texture_handle != NULL_HANDLE)
            {
                auto texture = InternalResourceHandleTable::Instance().GetTexture(m_final_color_output_texture_handle);
                m_final_color_output = texture->m_texture;
            }

            // find final color output and copy to swapchain buffer, only debug logic
            GLTF_CHECK(m_final_color_output);

            const unsigned window_width = m_window.GetWidth();
            const unsigned window_height = m_window.GetHeight();
            if (window_width == 0 || window_height == 0)
            {
                return;
            }

            const bool swapchain_resized = m_resource_allocator.ResizeSwapchainIfNeeded(window_width, window_height);
            const unsigned render_width = m_resource_allocator.GetCurrentSwapchain().GetWidth();
            const unsigned render_height = m_resource_allocator.GetCurrentSwapchain().GetHeight();
            const bool resized_window_resources = m_resource_allocator.ResizeWindowDependentRenderTargets(render_width, render_height);
         
            if (m_tick_callback)
            {
                m_tick_callback(interval);
            }

            if (m_debug_ui_enabled && m_debug_ui_initialized)
            {
                GLTF_CHECK(RHIUtilInstanceManager::Instance().NewGUIFrame());
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                if (m_debug_ui_callback)
                {
                    m_debug_ui_callback();
                }
            }

            if (!swapchain_resized && !resized_window_resources)
            {
                m_resource_allocator.WaitFrameRenderFinished();
            }
            ++m_frame_index;
            FlushDeferredResourceReleases(false);

            const unsigned back_buffer_count = m_resource_allocator.GetCurrentSwapchain().GetBackBufferCount();
            const unsigned profiler_slot_index = back_buffer_count > 0
                ? (m_resource_allocator.GetCurrentBackBufferIndex() % back_buffer_count)
                : 0;
            ResolveGPUProfilerFrame(profiler_slot_index);
             
            auto& command_list = m_resource_allocator.GetCommandListForRecordPassCommand();
            // Wait current frame available
            const bool acquire_succeeded = m_resource_allocator.GetCurrentSwapchain().AcquireNewFrame(m_resource_allocator.GetDevice());
            if (!acquire_succeeded)
            {
                m_resource_allocator.InvalidateSwapchainResizeRequest();
                m_resource_allocator.ResizeSwapchainIfNeeded(window_width, window_height);
                return;
            }

            std::vector<RenderGraphNodeHandle> nodes;
            nodes.reserve(m_render_graph_node_handles.size());
            for (auto handle : m_render_graph_node_handles)
            {
                nodes.push_back(handle);
            }

#if defined(_DEBUG)
            {
                std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_readers;
                std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_writers;
                for (auto handle : nodes)
                {
                    const auto& node_desc = m_render_graph_nodes[handle.value];
                    const auto access = CollectResourceAccess(node_desc);

                    for (const auto& resource : access.reads)
                    {
                        resource_readers[resource].insert(handle);
                    }
                    for (const auto& resource : access.writes)
                    {
                        resource_writers[resource].insert(handle);
                    }
                }

                std::map<RenderGraphNodeHandle, std::set<RenderGraphNodeHandle>> inferred_edges;
                std::map<std::pair<RenderGraphNodeHandle, RenderGraphNodeHandle>, std::vector<ResourceKey>> edge_resources;
                for (const auto& writer_pair : resource_writers)
                {
                    const auto& resource = writer_pair.first;
                    const auto& writers = writer_pair.second;
                    auto readers_it = resource_readers.find(resource);

                    std::set<RenderGraphNodeHandle> consumers = writers;
                    if (readers_it != resource_readers.end())
                    {
                        consumers.insert(readers_it->second.begin(), readers_it->second.end());
                    }

                    for (const auto writer : writers)
                    {
                        for (const auto consumer : consumers)
                        {
                            if (writer == consumer)
                            {
                                continue;
                            }

                            inferred_edges[writer].insert(consumer);
                            edge_resources[{writer, consumer}].push_back(resource);
                        }
                    }
                }

                std::size_t signature = 1469598103934665603ULL;
                for (const auto& edge_pair : edge_resources)
                {
                    HashCombine(signature, edge_pair.first.first.value);
                    HashCombine(signature, edge_pair.first.second.value);
                    for (const auto& resource : edge_pair.second)
                    {
                        HashCombine(signature, static_cast<std::size_t>(resource.kind));
                        HashCombine(signature, resource.value);
                    }
                }

                static std::size_t last_signature = 0;
                if (signature != last_signature)
                {
                    last_signature = signature;

                    for (const auto& edge_pair : edge_resources)
                    {
                        const auto from = edge_pair.first.first;
                        const auto to = edge_pair.first.second;
                        const auto& to_desc = m_render_graph_nodes[to.value];
                        if (!HasDependency(to_desc, from))
                        {
                            LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Node %u should depend on node %u (resources: %zu).\n",
                                to.value, from.value, edge_pair.second.size());
                        }
                    }

                    std::map<RenderGraphNodeHandle, unsigned> indegree;
                    for (auto handle : nodes)
                    {
                        indegree[handle] = 0;
                    }
                    for (const auto& edge_pair : inferred_edges)
                    {
                        for (const auto& to : edge_pair.second)
                        {
                            ++indegree[to];
                        }
                    }

                    std::vector<RenderGraphNodeHandle> queue;
                    for (const auto& count : indegree)
                    {
                        if (count.second == 0)
                        {
                            queue.push_back(count.first);
                        }
                    }

                    size_t processed = 0;
                    while (!queue.empty())
                    {
                        const auto node = queue.back();
                        queue.pop_back();
                        ++processed;

                        auto edges_it = inferred_edges.find(node);
                        if (edges_it == inferred_edges.end())
                        {
                            continue;
                        }
                        for (const auto& to : edges_it->second)
                        {
                            auto& count = indegree[to];
                            if (count > 0)
                            {
                                --count;
                                if (count == 0)
                                {
                                    queue.push_back(to);
                                }
                            }
                        }
                    }

                    if (processed != nodes.size())
                    {
                        LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Cycle detected in inferred dependencies. Remaining nodes:");
                        for (const auto& count : indegree)
                        {
                            if (count.second > 0)
                            {
                                LOG_FORMAT_FLUSH(" %u", count.first.value);
                            }
                        }
                        LOG_FORMAT_FLUSH("\n");
                    }
                }
            }
#endif
            std::size_t signature = 1469598103934665603ULL;
            for (auto handle : nodes)
            {
                HashCombine(signature, handle.value);
                const auto& node_desc = m_render_graph_nodes[handle.value];
                const auto access = CollectResourceAccess(node_desc);

                HashCombine(signature, 0xA11u);
                for (const auto& resource : access.reads)
                {
                    HashCombine(signature, static_cast<std::size_t>(resource.kind));
                    HashCombine(signature, resource.value);
                }

                HashCombine(signature, 0xB22u);
                for (const auto& resource : access.writes)
                {
                    HashCombine(signature, static_cast<std::size_t>(resource.kind));
                    HashCombine(signature, resource.value);
                }

                HashCombine(signature, 0xC33u);
                for (const auto dep : node_desc.dependency_render_graph_nodes)
                {
                    HashCombine(signature, dep.value);
                }
            }

            if (signature != m_cached_execution_signature || m_cached_execution_order.size() != nodes.size())
            {
                std::vector<RenderGraphNodeHandle> execution_nodes;
                std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_readers;
                std::map<ResourceKey, std::set<RenderGraphNodeHandle>> resource_writers;
                for (auto handle : nodes)
                {
                    const auto& node_desc = m_render_graph_nodes[handle.value];
                    const auto access = CollectResourceAccess(node_desc);
                    for (const auto& resource : access.reads)
                    {
                        resource_readers[resource].insert(handle);
                    }
                    for (const auto& resource : access.writes)
                    {
                        resource_writers[resource].insert(handle);
                    }
                }

                std::map<RenderGraphNodeHandle, std::set<RenderGraphNodeHandle>> inferred_edges;
                for (const auto& writer_pair : resource_writers)
                {
                    const auto& resource = writer_pair.first;
                    const auto& writers = writer_pair.second;
                    auto readers_it = resource_readers.find(resource);

                    std::set<RenderGraphNodeHandle> consumers = writers;
                    if (readers_it != resource_readers.end())
                    {
                        consumers.insert(readers_it->second.begin(), readers_it->second.end());
                    }

                    for (const auto writer : writers)
                    {
                        for (const auto consumer : consumers)
                        {
                            if (writer == consumer)
                            {
                                continue;
                            }
                            inferred_edges[writer].insert(consumer);
                        }
                    }
                }

                for (const auto handle : nodes)
                {
                    const auto& node_desc = m_render_graph_nodes[handle.value];
                    for (const auto dep : node_desc.dependency_render_graph_nodes)
                    {
                        if (dep.IsValid() && dep != handle)
                        {
                            inferred_edges[dep].insert(handle);
                        }
                    }
                }

                std::map<RenderGraphNodeHandle, unsigned> indegree;
                for (auto handle : nodes)
                {
                    indegree[handle] = 0;
                }
                for (const auto& edge_pair : inferred_edges)
                {
                    for (const auto& to : edge_pair.second)
                    {
                        ++indegree[to];
                    }
                }

                std::set<RenderGraphNodeHandle> ready;
                for (const auto& count : indegree)
                {
                    if (count.second == 0)
                    {
                        ready.insert(count.first);
                    }
                }

                while (!ready.empty())
                {
                    const auto node = *ready.begin();
                    ready.erase(ready.begin());
                    execution_nodes.push_back(node);

                    auto edges_it = inferred_edges.find(node);
                    if (edges_it == inferred_edges.end())
                    {
                        continue;
                    }
                    for (const auto& to : edges_it->second)
                    {
                        auto& count = indegree[to];
                        if (count > 0)
                        {
                            --count;
                            if (count == 0)
                            {
                                ready.insert(to);
                            }
                        }
                    }
                }

                if (execution_nodes.size() != nodes.size())
                {
                    LOG_FORMAT_FLUSH("[RenderGraph][Dependency] Cycle detected in inferred dependencies. Using registration order.\n");
                    execution_nodes = nodes;
                }

                m_cached_execution_order = std::move(execution_nodes);
                m_cached_execution_signature = signature;
            }

            FrameStats submitted_frame_stats{};
            submitted_frame_stats.frame_index = m_frame_index;
            submitted_frame_stats.cpu_total_ms = 0.0f;
            submitted_frame_stats.gpu_time_valid = false;
            submitted_frame_stats.gpu_total_ms = 0.0f;
            submitted_frame_stats.pass_stats.clear();
            submitted_frame_stats.pass_stats.reserve(m_cached_execution_order.size());

            GLTF_CHECK(BeginGPUProfilerFrame(command_list, profiler_slot_index));
            const unsigned max_timestamped_pass_count = (m_gpu_profiler_state && m_gpu_profiler_state->supported)
                ? m_gpu_profiler_state->max_pass_count
                : 0u;
            unsigned timestamped_pass_count = 0;

            for (unsigned pass_index = 0; pass_index < m_cached_execution_order.size(); ++pass_index)
            {
                auto render_graph_node = m_cached_execution_order[pass_index];
                const bool enable_gpu_timestamp = max_timestamped_pass_count > 0 && pass_index < max_timestamped_pass_count;
                if (enable_gpu_timestamp)
                {
                    const unsigned query_index = static_cast<unsigned>(pass_index * 2);
                    GLTF_CHECK(WriteGPUProfilerTimestamp(command_list, profiler_slot_index, query_index));
                }

                const auto pass_begin = std::chrono::steady_clock::now();
                ExecuteRenderGraphNode(command_list, render_graph_node, interval);
                const auto pass_end = std::chrono::steady_clock::now();
                const float pass_cpu_ms = std::chrono::duration<float, std::milli>(pass_end - pass_begin).count();

                if (enable_gpu_timestamp)
                {
                    const unsigned query_index = static_cast<unsigned>(pass_index * 2 + 1);
                    GLTF_CHECK(WriteGPUProfilerTimestamp(command_list, profiler_slot_index, query_index));
                    ++timestamped_pass_count;
                }

                const auto& node_desc = m_render_graph_nodes[render_graph_node.value];
                std::string group_name = node_desc.debug_group;
                if (group_name.empty())
                {
                    group_name = "Ungrouped";
                }

                std::string pass_name = node_desc.debug_name;
                if (pass_name.empty())
                {
                    auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(node_desc.render_pass_handle);
                    std::string type_name = render_pass ? ToRenderPassTypeName(render_pass->GetRenderPassType()) : "Unknown";
                    pass_name = type_name + "#" + std::to_string(render_graph_node.value);
                }

                RenderPassFrameStats pass_stats{};
                pass_stats.node_handle = render_graph_node;
                pass_stats.group_name = group_name;
                pass_stats.pass_name = pass_name;
                pass_stats.cpu_time_ms = pass_cpu_ms;
                submitted_frame_stats.pass_stats.push_back(pass_stats);
                submitted_frame_stats.cpu_total_ms += pass_cpu_ms;
            }

            GLTF_CHECK(FinalizeGPUProfilerFrame(command_list, profiler_slot_index, timestamped_pass_count * 2, submitted_frame_stats));
            if (!(m_gpu_profiler_state && m_gpu_profiler_state->supported))
            {
                m_last_frame_stats = submitted_frame_stats;
            }
            else if (timestamped_pass_count == 0)
            {
                m_last_frame_stats = submitted_frame_stats;
            }
            else if (m_last_frame_stats.pass_stats.empty())
            {
                m_last_frame_stats = submitted_frame_stats;
            }

            CollectUnusedRenderPassDescriptorResources();

            if (!m_render_graph_node_handles.empty())
            {
                auto src = m_final_color_output;
                auto dst = m_resource_allocator.GetCurrentSwapchainRT().m_source;

                src->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
                dst->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);

                const auto& src_desc = src->GetTextureDesc();
                const auto& dst_desc = dst->GetTextureDesc();
                const unsigned copy_width = (std::min)((std::min)(window_width, src_desc.GetTextureWidth()), dst_desc.GetTextureWidth());
                const unsigned copy_height = (std::min)((std::min)(window_height, src_desc.GetTextureHeight()), dst_desc.GetTextureHeight());
                if (copy_width > 0 && copy_height > 0)
                {
                    RHICopyTextureInfo copy_info{};
                    copy_info.copy_width = copy_width;
                    copy_info.copy_height = copy_height;
                    copy_info.dst_mip_level = 0;
                    copy_info.src_mip_level = 0;
                    copy_info.dst_x = 0;
                    copy_info.dst_y = 0;
                    RHIUtilInstanceManager::Instance().CopyTexture(command_list, *dst, *src, copy_info);
                }
            }

            GLTF_CHECK(RenderDebugUI(command_list));
                 
            Present(command_list);

            // Clear all nodes at end of frame
            m_render_graph_node_handles.clear();
        });

        return true;
    }

    void RenderGraph::RegisterTextureToColorOutput(TextureHandle texture_handle)
    {
        m_final_color_output_texture_handle = texture_handle;
        m_final_color_output_render_target_handle = NULL_HANDLE;
        auto texture = InternalResourceHandleTable::Instance().GetTexture(texture_handle);
        m_final_color_output = texture->m_texture;
    }

    void RenderGraph::RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle)
    {
        m_final_color_output_render_target_handle = render_target_handle;
        m_final_color_output_texture_handle = NULL_HANDLE;
        auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_handle);
        m_final_color_output = render_target->m_source;
    }

    void RenderGraph::RegisterTickCallback(const RenderGraphTickCallback& callback)
    {
        m_tick_callback = callback;
    }

    void RenderGraph::RegisterDebugUICallback(const RenderGraphDebugUICallback& callback)
    {
        m_debug_ui_callback = callback;
    }

    void RenderGraph::EnableDebugUI(bool enable)
    {
        m_debug_ui_enabled = enable;
    }

    const RenderGraph::FrameStats& RenderGraph::GetLastFrameStats() const
    {
        return m_last_frame_stats;
    }

    bool RenderGraph::InitDebugUI()
    {
        if (m_debug_ui_initialized)
        {
            return true;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        GLTF_CHECK(ImGui_ImplGlfw_InitForOther(glTFWindow::Get().GetGLFWWindow(), true));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().InitGUIContext(
            m_resource_allocator.GetDevice(),
            m_resource_allocator.GetCommandQueue(),
            m_resource_allocator.GetDescriptorManager(),
            m_resource_allocator.GetCurrentSwapchain().GetBackBufferCount()));

        glTFWindow::Get().SetInputHandleCallback([this]()
        {
            if (!m_debug_ui_enabled || !m_debug_ui_initialized)
            {
                return false;
            }
            const ImGuiIO& io = ImGui::GetIO();
            return io.WantCaptureMouse || io.WantCaptureKeyboard;
        });
        m_debug_ui_initialized = true;
        return true;
    }

    bool RenderGraph::RenderDebugUI(IRHICommandList& command_list)
    {
        if (!m_debug_ui_enabled || !m_debug_ui_initialized)
        {
            return true;
        }

        ImGui::Render();

        RHIBeginRenderingInfo begin_rendering_info{};
        const auto& swapchain_desc = m_resource_allocator.GetCurrentSwapchainRT().m_source->GetTextureDesc();
        begin_rendering_info.rendering_area_width = swapchain_desc.GetTextureWidth();
        begin_rendering_info.rendering_area_height = swapchain_desc.GetTextureHeight();
        begin_rendering_info.enable_depth_write = false;
        begin_rendering_info.clear_render_target = false;
        begin_rendering_info.m_render_targets = {&m_resource_allocator.GetCurrentSwapchainRT()};

        GLTF_CHECK(RHIUtilInstanceManager::Instance().BeginRendering(command_list, begin_rendering_info));
        GLTF_CHECK(m_resource_allocator.GetDescriptorManager().BindGUIDescriptorContext(command_list));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().RenderGUIFrame(command_list));
        GLTF_CHECK(RHIUtilInstanceManager::Instance().EndRendering(command_list));

        return true;
    }

    void RenderGraph::ShutdownDebugUI()
    {
        if (!m_debug_ui_initialized)
        {
            return;
        }

        m_debug_ui_callback = nullptr;
        m_debug_ui_enabled = false;
        glTFWindow::Get().SetInputHandleCallback([]() { return false; });
        RHIUtilInstanceManager::Instance().ExitGUI();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_debug_ui_initialized = false;
    }

    bool RenderGraph::InitGPUProfiler()
    {
        if (m_gpu_profiler_state)
        {
            return true;
        }

        m_gpu_profiler_state = std::make_unique<GPUProfilerState>();
        auto& profiler_state = *m_gpu_profiler_state;
        const unsigned back_buffer_count = m_resource_allocator.GetCurrentSwapchain().GetBackBufferCount();
        if (back_buffer_count == 0)
        {
            return true;
        }

        profiler_state.frame_slots.resize(back_buffer_count);
        
        GLTF_CHECK(RHIUtilInstanceManager::Instance().InitTimestampProfiler(
            m_resource_allocator.GetDevice(),
            m_resource_allocator.GetCommandQueue(),
            back_buffer_count,
            profiler_state.max_query_count));
        profiler_state.supported = RHIUtilInstanceManager::Instance().IsTimestampProfilerSupported();
        if (profiler_state.supported)
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Profiler] GPU timestamp profiler enabled.\n");
        }
        else
        {
            LOG_FORMAT_FLUSH("[RenderGraph][Profiler] GPU timestamp profiler unavailable, fallback to CPU timings only.\n");
        }

        return true;
    }

    void RenderGraph::ShutdownGPUProfiler()
    {
        if (!m_gpu_profiler_state)
        {
            return;
        }

        RHIUtilInstanceManager::Instance().ShutdownTimestampProfiler();
        m_gpu_profiler_state.reset();
    }

    void RenderGraph::ResolveGPUProfilerFrame(unsigned slot_index)
    {
        if (!m_gpu_profiler_state || !m_gpu_profiler_state->supported)
        {
            return;
        }
        if (slot_index >= m_gpu_profiler_state->frame_slots.size())
        {
            return;
        }

        auto& profiler_state = *m_gpu_profiler_state;
        auto& frame_slot = profiler_state.frame_slots[slot_index];
        if (!frame_slot.has_pending_frame_stats || frame_slot.query_count == 0)
        {
            return;
        }

        const unsigned query_count = frame_slot.query_count;
        std::vector<uint64_t> timestamps(query_count, 0);
        double ticks_per_second = 0.0;
        const bool readback_ok = RHIUtilInstanceManager::Instance().ResolveTimestampFrame(
            slot_index,
            query_count,
            timestamps,
            ticks_per_second);

        if (!readback_ok)
        {
            frame_slot.has_pending_frame_stats = false;
            frame_slot.query_count = 0;
            return;
        }

        auto resolved_stats = frame_slot.pending_frame_stats;
        resolved_stats.gpu_time_valid = false;
        resolved_stats.gpu_total_ms = 0.0f;

        const unsigned timed_pass_count = (std::min)(static_cast<unsigned>(resolved_stats.pass_stats.size()), query_count / 2);
        for (unsigned i = 0; i < timed_pass_count; ++i)
        {
            const uint64_t begin_tick = timestamps[i * 2];
            const uint64_t end_tick = timestamps[i * 2 + 1];
            if (end_tick <= begin_tick)
            {
                continue;
            }

            if (ticks_per_second <= 0.0)
            {
                break;
            }

            const double gpu_time_ms = static_cast<double>(end_tick - begin_tick) * 1000.0 / ticks_per_second;

            auto& pass_stats = resolved_stats.pass_stats[i];
            pass_stats.gpu_time_valid = true;
            pass_stats.gpu_time_ms = static_cast<float>(gpu_time_ms);
            resolved_stats.gpu_total_ms += static_cast<float>(gpu_time_ms);
            resolved_stats.gpu_time_valid = true;
        }

        m_last_frame_stats = resolved_stats;
        frame_slot.has_pending_frame_stats = false;
        frame_slot.query_count = 0;
    }

    bool RenderGraph::BeginGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index)
    {
        if (!m_gpu_profiler_state || !m_gpu_profiler_state->supported)
        {
            return true;
        }
        if (slot_index >= m_gpu_profiler_state->frame_slots.size())
        {
            return true;
        }

        GLTF_CHECK(RHIUtilInstanceManager::Instance().BeginTimestampFrame(command_list, slot_index));
        return true;
    }

    bool RenderGraph::WriteGPUProfilerTimestamp(IRHICommandList& command_list, unsigned slot_index, unsigned query_index)
    {
        if (!m_gpu_profiler_state || !m_gpu_profiler_state->supported)
        {
            return true;
        }
        if (slot_index >= m_gpu_profiler_state->frame_slots.size())
        {
            return true;
        }
        if (query_index >= m_gpu_profiler_state->max_query_count)
        {
            return true;
        }

        GLTF_CHECK(RHIUtilInstanceManager::Instance().WriteTimestamp(command_list, slot_index, query_index));
        return true;
    }

    bool RenderGraph::FinalizeGPUProfilerFrame(IRHICommandList& command_list, unsigned slot_index, unsigned query_count, const FrameStats& frame_stats)
    {
        if (!m_gpu_profiler_state || !m_gpu_profiler_state->supported)
        {
            return true;
        }
        if (slot_index >= m_gpu_profiler_state->frame_slots.size())
        {
            return true;
        }

        auto& profiler_state = *m_gpu_profiler_state;
        auto& frame_slot = profiler_state.frame_slots[slot_index];
        const unsigned clamped_query_count = (std::min)(query_count, profiler_state.max_query_count);
        GLTF_CHECK(RHIUtilInstanceManager::Instance().EndTimestampFrame(command_list, slot_index, clamped_query_count));

        frame_slot.query_count = clamped_query_count;
        frame_slot.pending_frame_stats = frame_stats;
        frame_slot.has_pending_frame_stats = clamped_query_count > 0;
        return true;
    }

    void RenderGraph::EnqueueResourceForDeferredRelease(const std::shared_ptr<IRHIResource>& resource)
    {
        if (!resource)
        {
            return;
        }

        const unsigned delay_frame = (std::max)(2u, m_resource_allocator.GetCurrentSwapchain().GetBackBufferCount());
        if (!m_deferred_release_entries.empty() && m_deferred_release_entries.back().retire_frame == m_frame_index + delay_frame)
        {
            m_deferred_release_entries.back().resources.push_back(resource);
            return;
        }

        DeferredReleaseEntry entry{};
        entry.retire_frame = m_frame_index + delay_frame;
        entry.resources.push_back(resource);
        m_deferred_release_entries.push_back(std::move(entry));
    }

    void RenderGraph::EnqueueBufferDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name)
    {
        const auto descriptor_it = descriptor_resource.m_buffer_descriptors.find(binding_name);
        if (descriptor_it != descriptor_resource.m_buffer_descriptors.end())
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_it->second));
            descriptor_resource.m_buffer_descriptors.erase(descriptor_it);
        }
        descriptor_resource.m_cached_buffer_bindings.erase(binding_name);
    }

    void RenderGraph::EnqueueTextureDescriptorForDeferredRelease(RenderPassDescriptorResource& descriptor_resource, const std::string& binding_name)
    {
        const auto descriptor_it = descriptor_resource.m_texture_descriptors.find(binding_name);
        if (descriptor_it != descriptor_resource.m_texture_descriptors.end())
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_it->second));
            descriptor_resource.m_texture_descriptors.erase(descriptor_it);
        }

        const auto descriptor_table_it = descriptor_resource.m_texture_descriptor_tables.find(binding_name);
        if (descriptor_table_it != descriptor_resource.m_texture_descriptor_tables.end())
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_table_it->second));
            descriptor_resource.m_texture_descriptor_tables.erase(descriptor_table_it);
        }

        const auto table_source_it = descriptor_resource.m_texture_descriptor_table_source_data.find(binding_name);
        if (table_source_it != descriptor_resource.m_texture_descriptor_table_source_data.end())
        {
            for (const auto& descriptor : table_source_it->second)
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor));
            }
            descriptor_resource.m_texture_descriptor_table_source_data.erase(table_source_it);
        }

        descriptor_resource.m_cached_texture_bindings.erase(binding_name);
        descriptor_resource.m_cached_render_target_texture_bindings.erase(binding_name);
    }

    void RenderGraph::ReleaseRenderPassDescriptorResource(RenderPassDescriptorResource& descriptor_resource)
    {
        for (const auto& descriptor_pair : descriptor_resource.m_buffer_descriptors)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_pair.second));
        }
        for (const auto& descriptor_pair : descriptor_resource.m_texture_descriptors)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_pair.second));
        }
        for (const auto& descriptor_table_pair : descriptor_resource.m_texture_descriptor_tables)
        {
            EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor_table_pair.second));
        }
        for (const auto& table_source_pair : descriptor_resource.m_texture_descriptor_table_source_data)
        {
            for (const auto& descriptor : table_source_pair.second)
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor));
            }
        }

        descriptor_resource.m_buffer_descriptors.clear();
        descriptor_resource.m_texture_descriptors.clear();
        descriptor_resource.m_texture_descriptor_tables.clear();
        descriptor_resource.m_texture_descriptor_table_source_data.clear();
        descriptor_resource.m_cached_buffer_bindings.clear();
        descriptor_resource.m_cached_texture_bindings.clear();
        descriptor_resource.m_cached_render_target_texture_bindings.clear();
    }

    void RenderGraph::PruneDescriptorResources(RenderPassDescriptorResource& descriptor_resource, const RenderPassDrawDesc& draw_info)
    {
        for (auto it = descriptor_resource.m_buffer_descriptors.begin(); it != descriptor_resource.m_buffer_descriptors.end(); )
        {
            if (!draw_info.buffer_resources.contains(it->first))
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(it->second));
                it = descriptor_resource.m_buffer_descriptors.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = descriptor_resource.m_cached_buffer_bindings.begin(); it != descriptor_resource.m_cached_buffer_bindings.end(); )
        {
            if (!draw_info.buffer_resources.contains(it->first))
            {
                it = descriptor_resource.m_cached_buffer_bindings.erase(it);
            }
            else
            {
                ++it;
            }
        }

        const auto should_keep_texture_binding = [&draw_info](const std::string& binding_name)
        {
            return draw_info.texture_resources.contains(binding_name) || draw_info.render_target_texture_resources.contains(binding_name);
        };

        for (auto it = descriptor_resource.m_texture_descriptors.begin(); it != descriptor_resource.m_texture_descriptors.end(); )
        {
            if (!should_keep_texture_binding(it->first))
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(it->second));
                it = descriptor_resource.m_texture_descriptors.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = descriptor_resource.m_texture_descriptor_tables.begin(); it != descriptor_resource.m_texture_descriptor_tables.end(); )
        {
            if (!should_keep_texture_binding(it->first))
            {
                EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(it->second));
                it = descriptor_resource.m_texture_descriptor_tables.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = descriptor_resource.m_texture_descriptor_table_source_data.begin(); it != descriptor_resource.m_texture_descriptor_table_source_data.end(); )
        {
            if (!should_keep_texture_binding(it->first))
            {
                for (const auto& descriptor : it->second)
                {
                    EnqueueResourceForDeferredRelease(std::static_pointer_cast<IRHIResource>(descriptor));
                }
                it = descriptor_resource.m_texture_descriptor_table_source_data.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = descriptor_resource.m_cached_texture_bindings.begin(); it != descriptor_resource.m_cached_texture_bindings.end(); )
        {
            if (!draw_info.texture_resources.contains(it->first))
            {
                it = descriptor_resource.m_cached_texture_bindings.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = descriptor_resource.m_cached_render_target_texture_bindings.begin(); it != descriptor_resource.m_cached_render_target_texture_bindings.end(); )
        {
            if (!draw_info.render_target_texture_resources.contains(it->first))
            {
                it = descriptor_resource.m_cached_render_target_texture_bindings.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void RenderGraph::CollectUnusedRenderPassDescriptorResources()
    {
        const unsigned retention_frame = (std::max)(2u, m_resource_allocator.GetCurrentSwapchain().GetBackBufferCount());
        for (auto it = m_render_pass_descriptor_resources.begin(); it != m_render_pass_descriptor_resources.end(); )
        {
            const auto node_handle = it->first;
            if (m_render_graph_node_handles.contains(node_handle))
            {
                ++it;
                continue;
            }

            const auto last_used_it = m_render_pass_descriptor_last_used_frame.find(node_handle);
            const unsigned long long last_used_frame = last_used_it != m_render_pass_descriptor_last_used_frame.end() ? last_used_it->second : 0;
            if (m_frame_index < last_used_frame + retention_frame)
            {
                ++it;
                continue;
            }

            ReleaseRenderPassDescriptorResource(it->second);
            it = m_render_pass_descriptor_resources.erase(it);
            m_render_pass_descriptor_last_used_frame.erase(node_handle);
        }
    }

    void RenderGraph::FlushDeferredResourceReleases(bool force_release_all)
    {
        auto& memory_manager = m_resource_allocator.GetMemoryManager();
        while (!m_deferred_release_entries.empty())
        {
            if (!force_release_all && m_deferred_release_entries.front().retire_frame > m_frame_index)
            {
                break;
            }

            auto entry = std::move(m_deferred_release_entries.front());
            m_deferred_release_entries.pop_front();
            for (const auto& resource : entry.resources)
            {
                const bool released = RHIResourceFactory::ReleaseResource(memory_manager, resource);
                GLTF_CHECK(released);
            }
        }
    }

    void RenderGraph::ExecuteRenderGraphNode(IRHICommandList& command_list, RenderGraphNodeHandle render_graph_node_handle, unsigned long long interval)
    {
        GLTF_CHECK(render_graph_node_handle.IsValid());
        GLTF_CHECK(render_graph_node_handle.value < m_render_graph_nodes.size());
        auto& render_graph_node_desc = m_render_graph_nodes[render_graph_node_handle.value];
        if (render_graph_node_desc.pre_render_callback)
        {
            render_graph_node_desc.pre_render_callback(interval);
        }
        
        auto render_pass = InternalResourceHandleTable::Instance().GetRenderPass(render_graph_node_desc.render_pass_handle);

        RHIUtilInstanceManager::Instance().SetPipelineState(command_list, render_pass->GetPipelineStateObject());
        RHIUtilInstanceManager::Instance().SetRootSignature(command_list, render_pass->GetRootSignature(), render_pass->GetPipelineStateObject(), RendererInterfaceRHIConverter::ConvertToRHIPipelineType(render_pass->GetRenderPassType()));
        RHIUtilInstanceManager::Instance().SetPrimitiveTopology(command_list, ConvertToRHIPrimitiveTopology(render_pass->GetPrimitiveTopology()));

        unsigned default_viewport_width = m_window.GetWidth();
        unsigned default_viewport_height = m_window.GetHeight();
        if (!render_graph_node_desc.draw_info.render_target_resources.empty())
        {
            const auto first_render_target_handle = render_graph_node_desc.draw_info.render_target_resources.begin()->first;
            auto first_render_target = InternalResourceHandleTable::Instance().GetRenderTarget(first_render_target_handle);
            default_viewport_width = first_render_target->m_source->GetTextureDesc().GetTextureWidth();
            default_viewport_height = first_render_target->m_source->GetTextureDesc().GetTextureHeight();

            for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
            {
                auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
                default_viewport_width = (std::min)(default_viewport_width, render_target->m_source->GetTextureDesc().GetTextureWidth());
                default_viewport_height = (std::min)(default_viewport_height, render_target->m_source->GetTextureDesc().GetTextureHeight());
            }
        }

        RHIViewportDesc viewport{};
        viewport.width = render_pass->GetViewportSize().first >= 0 ? render_pass->GetViewportSize().first : default_viewport_width;
        viewport.height = render_pass->GetViewportSize().second >= 0 ? render_pass->GetViewportSize().second : default_viewport_height;
        viewport.min_depth = 0.f;
        viewport.max_depth = 1.f;
        viewport.top_left_x = 0.f;
        viewport.top_left_y = 0.f;
        RHIUtilInstanceManager::Instance().SetViewport(command_list, viewport);

        const RHIScissorRectDesc scissor_rect =
            {
            (unsigned)viewport.top_left_x,
            (unsigned)viewport.top_left_y,
            (unsigned)(viewport.top_left_x + viewport.width),
            (unsigned)(viewport.top_left_y + viewport.height)
        }; 
        RHIUtilInstanceManager::Instance().SetScissorRect(command_list, scissor_rect);

        RHIBeginRenderingInfo begin_rendering_info{};
        
        // render target binding
        bool clear_render_target = false;
        bool clear_depth_stencil = false;
        for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
        {
            GLTF_CHECK(render_target_info.first != NULL_HANDLE);
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
            begin_rendering_info.m_render_targets.push_back(render_target.get());

            RenderPassAttachmentLoadOp load_op = render_target_info.second.load_op;
            if (render_target_info.second.need_clear && load_op == RenderPassAttachmentLoadOp::LOAD)
            {
                load_op = RenderPassAttachmentLoadOp::CLEAR;
            }

            RHIAttachmentLoadOp rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_LOAD;
            switch (load_op)
            {
            case RenderPassAttachmentLoadOp::LOAD:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_LOAD;
                break;
            case RenderPassAttachmentLoadOp::CLEAR:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_CLEAR;
                break;
            case RenderPassAttachmentLoadOp::DONT_CARE:
                rhi_load_op = RHIAttachmentLoadOp::LOAD_OP_DONT_CARE;
                break;
            }

            RHIAttachmentStoreOp rhi_store_op = RHIAttachmentStoreOp::STORE_OP_STORE;
            switch (render_target_info.second.store_op)
            {
            case RenderPassAttachmentStoreOp::STORE:
                rhi_store_op = RHIAttachmentStoreOp::STORE_OP_STORE;
                break;
            case RenderPassAttachmentStoreOp::DONT_CARE:
                rhi_store_op = RHIAttachmentStoreOp::STORE_OP_DONT_CARE;
                break;
            }

            begin_rendering_info.m_render_target_load_ops.push_back(rhi_load_op);
            begin_rendering_info.m_render_target_store_ops.push_back(rhi_store_op);

            if (rhi_load_op == RHIAttachmentLoadOp::LOAD_OP_CLEAR)
            {
                if (render_target_info.second.usage == RenderPassResourceUsage::DEPTH_STENCIL)
                {
                    clear_depth_stencil = true;
                    begin_rendering_info.enable_depth_write = true;
                }
                else
                {
                    clear_render_target = true;
                }
            }
        }

        begin_rendering_info.rendering_area_offset_x = viewport.top_left_x;
        begin_rendering_info.rendering_area_offset_y = viewport.top_left_y;
        begin_rendering_info.rendering_area_width = viewport.width;
        begin_rendering_info.rendering_area_height = viewport.height;
        begin_rendering_info.clear_render_target = clear_render_target;
        begin_rendering_info.clear_depth_stencil = clear_depth_stencil;

        // Bind descriptor heap
        m_resource_allocator.GetDescriptorManager().BindDescriptorContext(command_list);

        // buffer binding
        RHIPipelineType pipeline_type = RHIPipelineType::Unknown;
        switch (render_pass->GetRenderPassType()) {
        case RenderPassType::GRAPHICS:
            pipeline_type = RHIPipelineType::Graphics;
            break;
        case RenderPassType::COMPUTE:
            pipeline_type = RHIPipelineType::Compute;
            break;
        case RenderPassType::RAY_TRACING:
            pipeline_type = RHIPipelineType::RayTracing;
            break;
        }

        auto& render_pass_descriptor_resource = m_render_pass_descriptor_resources[render_graph_node_handle];
        m_render_pass_descriptor_last_used_frame[render_graph_node_handle] = m_frame_index;
        PruneDescriptorResources(render_pass_descriptor_resource, render_graph_node_desc.draw_info);
        
        for (const auto& buffer : render_graph_node_desc.draw_info.buffer_resources)
        {
            GLTF_CHECK(buffer.second.buffer_handle != NULL_HANDLE);
            auto& root_signature_allocation = render_pass->GetRootSignatureAllocation(buffer.first);
            auto buffer_handle = buffer.second.buffer_handle;
            auto buffer_allocation = RendererInterface::InternalResourceHandleTable::Instance().GetBuffer(buffer_handle);
            auto buffer_size = buffer_allocation->m_buffer->GetBufferDesc().width;

            const bool need_recreate_descriptor =
                !render_pass_descriptor_resource.m_buffer_descriptors.contains(buffer.first) ||
                !render_pass_descriptor_resource.m_cached_buffer_bindings.contains(buffer.first) ||
                !IsSameBufferBindingDesc(render_pass_descriptor_resource.m_cached_buffer_bindings.at(buffer.first), buffer.second);

            if (need_recreate_descriptor)
            {
                EnqueueBufferDescriptorForDeferredRelease(render_pass_descriptor_resource, buffer.first);
                switch (buffer.second.binding_type)
                {
                case BufferBindingDesc::CBV:
                    {
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_CBV, buffer_size, 0);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, render_pass_descriptor_resource.m_buffer_descriptors[buffer.first]);    
                    }
                    break;
                case BufferBindingDesc::SRV:
                    {
                        RHISRVStructuredBufferDesc srv_buffer_desc{buffer.second.stride, buffer.second.count, buffer.second.is_structured_buffer};
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_SRV, buffer_size, 0, srv_buffer_desc);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, render_pass_descriptor_resource.m_buffer_descriptors[buffer.first]);    
                    }
                    break;
                case BufferBindingDesc::UAV:
                    {
                        RHIUAVStructuredBufferDesc uav_buffer_desc{buffer.second.stride, buffer.second.count, buffer.second.is_structured_buffer, buffer.second.use_count_buffer, buffer.second.count_buffer_offset};
                        RHIBufferDescriptorDesc buffer_descriptor_desc(RHIDataFormat::UNKNOWN, RHIViewType::RVT_UAV, buffer_size, 0, uav_buffer_desc);
                        m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), buffer_allocation->m_buffer, buffer_descriptor_desc, render_pass_descriptor_resource.m_buffer_descriptors[buffer.first]);    
                    }
                    break;
                }

                render_pass_descriptor_resource.m_cached_buffer_bindings[buffer.first] = buffer.second;
            }

            auto buffer_descriptor = render_pass_descriptor_resource.m_buffer_descriptors.at(buffer.first);
            switch (buffer.second.binding_type) {
            case BufferBindingDesc::CBV:
                buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_VERTEX_AND_CONSTANT_BUFFER);
                break;
            case BufferBindingDesc::SRV:
                //buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
                //break;
            case BufferBindingDesc::UAV:
                buffer_allocation->m_buffer->Transition(command_list, RHIResourceStateType::STATE_ALL_SHADER_RESOURCE);
                break;
            }
            render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *buffer_descriptor);
        }

        for (const auto& texture  :render_graph_node_desc.draw_info.texture_resources)
        {
            GLTF_CHECK(!texture.second.textures.empty());
            const bool is_texture_table = texture.second.textures.size() > 1;
            auto& root_signature_allocation = render_pass->GetRootSignatureAllocation(texture.first);
            
            const bool need_recreate_descriptor =
                (!render_pass_descriptor_resource.m_texture_descriptors.contains(texture.first) && !render_pass_descriptor_resource.m_texture_descriptor_tables.contains(texture.first)) ||
                !render_pass_descriptor_resource.m_cached_texture_bindings.contains(texture.first) ||
                !IsSameTextureBindingDesc(render_pass_descriptor_resource.m_cached_texture_bindings.at(texture.first), texture.second);

            if (need_recreate_descriptor)
            {
                EnqueueTextureDescriptorForDeferredRelease(render_pass_descriptor_resource, texture.first);

                // Create texture descriptor
                auto texture_handles = texture.second.textures;
                
                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_allocations;
                for (const auto handle : texture_handles)
                {
                    auto texture_allocation = InternalResourceHandleTable::Instance().GetTexture(handle);
                    RHITextureDescriptorDesc texture_descriptor_desc{texture_allocation->m_texture->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, texture.second.type == TextureBindingDesc::SRV? RHIViewType::RVT_SRV :RHIViewType::RVT_UAV};
                    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor = nullptr;
                    m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), texture_allocation->m_texture, texture_descriptor_desc, texture_descriptor);
                    descriptor_allocations.push_back(texture_descriptor);
                }

                if (is_texture_table)
                {
                    std::shared_ptr<IRHIDescriptorTable> descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
                    const bool built = descriptor_table->Build(m_resource_allocator.GetDevice(), descriptor_allocations);
                    GLTF_CHECK(built);

                    render_pass_descriptor_resource.m_texture_descriptor_tables[texture.first] = descriptor_table;
                    render_pass_descriptor_resource.m_texture_descriptor_table_source_data[texture.first] = descriptor_allocations;
                }
                else
                {
                    GLTF_CHECK(descriptor_allocations.size() == 1);
                    render_pass_descriptor_resource.m_texture_descriptors[texture.first] = descriptor_allocations.at(0);                
                }

                render_pass_descriptor_resource.m_cached_texture_bindings[texture.first] = texture.second;
            }

            if (is_texture_table)
            {
                auto descriptor_table = render_pass_descriptor_resource.m_texture_descriptor_tables.at(texture.first);
                auto& table_texture_descriptors = render_pass_descriptor_resource.m_texture_descriptor_table_source_data.at(texture.first);
                for (const auto& table_texture : table_texture_descriptors)
                {
                    table_texture->m_source->Transition(command_list, texture.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }
                
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor_table, texture.second.type == TextureBindingDesc::SRV? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV);
            }
            else
            {
                auto descriptor = render_pass_descriptor_resource.m_texture_descriptors.at(texture.first);
                descriptor->m_source->Transition(command_list, texture.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor);
            }
        }

        for (const auto& render_target_pair  :render_graph_node_desc.draw_info.render_target_texture_resources)
        {
            GLTF_CHECK(!render_target_pair.second.render_target_texture.empty());
            const bool is_texture_table = render_target_pair.second.render_target_texture.size() > 1;
            bool need_recreate_descriptor =
                (!render_pass_descriptor_resource.m_texture_descriptors.contains(render_target_pair.first) &&
                    !render_pass_descriptor_resource.m_texture_descriptor_tables.contains(render_target_pair.first)) ||
                !render_pass_descriptor_resource.m_cached_render_target_texture_bindings.contains(render_target_pair.first) ||
                !IsSameRenderTargetTextureBindingDesc(render_pass_descriptor_resource.m_cached_render_target_texture_bindings.at(render_target_pair.first), render_target_pair.second);

            if (!need_recreate_descriptor)
            {
                if (is_texture_table)
                {
                    if (!render_pass_descriptor_resource.m_texture_descriptor_table_source_data.contains(render_target_pair.first))
                    {
                        need_recreate_descriptor = true;
                    }
                    else
                    {
                        const auto& descriptor_source_data = render_pass_descriptor_resource.m_texture_descriptor_table_source_data.at(render_target_pair.first);
                        if (descriptor_source_data.size() != render_target_pair.second.render_target_texture.size())
                        {
                            need_recreate_descriptor = true;
                        }
                        else
                        {
                            for (std::size_t descriptor_index = 0; descriptor_index < descriptor_source_data.size(); ++descriptor_index)
                            {
                                auto latest_render_target =
                                    InternalResourceHandleTable::Instance().GetRenderTarget(
                                        render_target_pair.second.render_target_texture[descriptor_index]);
                                if (descriptor_source_data[descriptor_index]->m_source != latest_render_target->m_source)
                                {
                                    need_recreate_descriptor = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (!render_pass_descriptor_resource.m_texture_descriptors.contains(render_target_pair.first))
                    {
                        need_recreate_descriptor = true;
                    }
                    else
                    {
                        auto latest_render_target =
                            InternalResourceHandleTable::Instance().GetRenderTarget(
                                render_target_pair.second.render_target_texture[0]);
                        auto current_descriptor =
                            render_pass_descriptor_resource.m_texture_descriptors.at(render_target_pair.first);
                        if (current_descriptor->m_source != latest_render_target->m_source)
                        {
                            need_recreate_descriptor = true;
                        }
                    }
                }
            }

            if (need_recreate_descriptor)
            {
                EnqueueTextureDescriptorForDeferredRelease(render_pass_descriptor_resource, render_target_pair.first);

                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_allocations;

                for (const auto& render_target : render_target_pair.second.render_target_texture)
                {
                    auto texture = InternalResourceHandleTable::Instance().GetRenderTarget(render_target)->m_source;
                    RHITextureDescriptorDesc texture_descriptor_desc{
                        texture->GetTextureFormat(),
                        RHIResourceDimension::TEXTURE2D,
                        render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV ? RHIViewType::RVT_SRV : RHIViewType::RVT_UAV};
                    if (IsDepthStencilFormat(texture->GetTextureFormat()) && render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV)
                    {
                        texture_descriptor_desc.m_format = RHIDataFormat::D32_SAMPLE_RESERVED;
                    }
                    std::shared_ptr<IRHITextureDescriptorAllocation> texture_descriptor = nullptr;
                    m_resource_allocator.GetDescriptorManager().CreateDescriptor(m_resource_allocator.GetDevice(), texture, texture_descriptor_desc, texture_descriptor);
                    descriptor_allocations.push_back(texture_descriptor);
                }
                
                if (is_texture_table)
                {
                    std::shared_ptr<IRHIDescriptorTable> descriptor_table = RHIResourceFactory::CreateRHIResource<IRHIDescriptorTable>();
                    const bool built = descriptor_table->Build(m_resource_allocator.GetDevice(), descriptor_allocations);
                    GLTF_CHECK(built);

                    render_pass_descriptor_resource.m_texture_descriptor_tables[render_target_pair.first] = descriptor_table;
                    render_pass_descriptor_resource.m_texture_descriptor_table_source_data[render_target_pair.first] = descriptor_allocations;
                }
                else
                {
                    GLTF_CHECK(render_target_pair.second.render_target_texture.size() == 1);
                    render_pass_descriptor_resource.m_texture_descriptors[render_target_pair.first] = descriptor_allocations[0];
                }

                render_pass_descriptor_resource.m_cached_render_target_texture_bindings[render_target_pair.first] = render_target_pair.second;
            }

            auto& root_signature_allocation = render_pass->GetRootSignatureAllocation(render_target_pair.first);
            if (is_texture_table)
            {
                auto descriptor_table = render_pass_descriptor_resource.m_texture_descriptor_tables.at(render_target_pair.first);
                auto& table_texture_descriptors = render_pass_descriptor_resource.m_texture_descriptor_table_source_data.at(render_target_pair.first);
                for (const auto& table_texture : table_texture_descriptors)
                {
                    table_texture->m_source->Transition(command_list, render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }
                
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor_table, render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV);
            }
            else
            {
                auto descriptor = render_pass_descriptor_resource.m_texture_descriptors.at(render_target_pair.first);
                descriptor->m_source->Transition(command_list, render_target_pair.second.type == RenderTargetTextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor);    
            }
        }

        render_pass->GetDescriptorUpdater().FinalizeUpdateDescriptors(m_resource_allocator.GetDevice(), command_list, render_pass->GetRootSignature());

        if (pipeline_type == RHIPipelineType::Graphics)
        {
            RHIUtilInstanceManager::Instance().BeginRendering(command_list, begin_rendering_info);    
        }
        
        const auto& draw_info = render_graph_node_desc.draw_info;
        for (const auto& command : draw_info.execute_commands)
        {
            switch (command.type) {
            case ExecuteCommandType::DRAW_VERTEX_COMMAND:
                RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                    command.parameter.draw_vertex_command_parameter.vertex_count,
                    1,
                    command.parameter.draw_vertex_command_parameter.start_vertex_location,
                    0);
                break;
            case ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND:
                RHIUtilInstanceManager::Instance().DrawInstanced(command_list,
                    command.parameter.draw_vertex_instance_command_parameter.vertex_count,
                    command.parameter.draw_vertex_instance_command_parameter.instance_count,
                    command.parameter.draw_vertex_instance_command_parameter.start_vertex_location,
                    command.parameter.draw_vertex_instance_command_parameter.start_instance_location);
                break;
            case ExecuteCommandType::DRAW_INDEXED_COMMAND:
                break;
            case ExecuteCommandType::DRAW_INDEXED_INSTANCING_COMMAND:
                {
                    auto indexed_buffer_view = InternalResourceHandleTable::Instance().GetIndexBufferView(command.input_buffer.index_buffer_handle);
                    //auto indexed_buffer = InternalResourceHandleTable::Instance().GetIndexBuffer(command.input_buffer.index_buffer_handle);
                    //indexed_buffer->GetBuffer().Transition(command_list, RHIResourceStateType::STATE_INDEX_BUFFER);
                    RHIUtilInstanceManager::Instance().SetIndexBufferView(command_list, *indexed_buffer_view);
                    RHIUtilInstanceManager::Instance().DrawIndexInstanced(command_list,
                        command.parameter.draw_indexed_instance_command_parameter.index_count_per_instance,
                        command.parameter.draw_indexed_instance_command_parameter.instance_count,
                        command.parameter.draw_indexed_instance_command_parameter.start_index_location,
                        command.parameter.draw_indexed_instance_command_parameter.start_vertex_location,
                        command.parameter.draw_indexed_instance_command_parameter.start_instance_location);
                    break;    
                }
            case ExecuteCommandType::COMPUTE_DISPATCH_COMMAND:
                RHIUtilInstanceManager::Instance().Dispatch(command_list, command.parameter.dispatch_parameter.group_size_x, command.parameter.dispatch_parameter.group_size_y, command.parameter.dispatch_parameter.group_size_z);
                break;
            case ExecuteCommandType::RAY_TRACING_COMMAND:
                break;
            }    
        }
        
        if (pipeline_type == RHIPipelineType::Graphics)
        {
            RHIUtilInstanceManager::Instance().EndRendering(command_list);
        }
    }

    void RenderGraph::CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait)
    {
        const bool closed = RHIUtilInstanceManager::Instance().CloseCommandList(command_list);
        GLTF_CHECK(closed);
        
        auto& command_queue = m_resource_allocator.GetCommandQueue();
        
        GLTF_CHECK(RHIUtilInstanceManager::Instance().ExecuteCommandList(command_list, command_queue, context));
        if (wait)
        {
            RHIUtilInstanceManager::Instance().WaitCommandListFinish(command_list);
        }
    }

    void RenderGraph::Present(IRHICommandList& command_list)
    {
        m_resource_allocator.GetCurrentSwapchainRT().m_source->Transition(command_list, RHIResourceStateType::STATE_PRESENT);

        RHIExecuteCommandListContext context;
        context.wait_infos.push_back({&m_resource_allocator.GetCurrentSwapchain().GetAvailableFrameSemaphore(), RHIPipelineStage::COLOR_ATTACHMENT_OUTPUT});
        context.sign_semaphores.push_back(&command_list.GetSemaphore());
        CloseCurrentCommandListAndExecute(command_list, context, false);
    
        const bool present_succeeded = RHIUtilInstanceManager::Instance().Present(
            m_resource_allocator.GetCurrentSwapchain(),
            m_resource_allocator.GetCommandQueue(),
            m_resource_allocator.GetCommandListForRecordPassCommand(NULL_HANDLE));
        if (!present_succeeded)
        {
            m_resource_allocator.InvalidateSwapchainResizeRequest();
            return;
        }
        CloseCurrentCommandListAndExecute(command_list, {}, false);
    }
}


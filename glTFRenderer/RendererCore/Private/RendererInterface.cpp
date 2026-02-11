#include "RendererInterface.h"

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

namespace RendererInterface
{
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
        return m_desc.width;
    }

    unsigned RenderWindow::GetHeight() const
    {
        return m_desc.height;
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
            static_cast<RHIResourceUsageFlags>(RUF_ALLOW_SRV | RUF_TRANSFER_DST),
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

    IRHICommandList& ResourceOperator::GetCommandListForRecordPassCommand(RenderPassHandle pass) const
    {
        return m_resource_manager->GetCommandListForRecordPassCommand(pass);
    }

    IRHIDescriptorManager& ResourceOperator::GetDescriptorManager() const
    {
        return m_resource_manager->GetMemoryManager().GetDescriptorManager();
    }

    IRHICommandQueue& ResourceOperator::GetCommandQueue() const
    {
        return m_resource_manager->GetCommandQueue();
    }

    IRHISwapChain& ResourceOperator::GetCurrentSwapchain() const
    {
        return m_resource_manager->GetSwapChain();
    }

    RenderGraph::RenderGraph(ResourceOperator& allocator, RenderWindow& window)
        : m_resource_allocator(allocator)
        , m_window(window)
    {
        
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
    
        if (setup_info.execute_command.has_value())
        {
            render_pass_draw_desc.execute_commands.push_back(setup_info.execute_command.value());    
        }

        render_pass_draw_desc.buffer_resources.insert(setup_info.buffer_resources.begin(), setup_info.buffer_resources.end());

        render_pass_desc.viewport_width = setup_info.viewport_width;
        render_pass_desc.viewport_height = setup_info.viewport_height;
        
        auto render_pass_handle = allocator.CreateRenderPass(render_pass_desc);
    
        RendererInterface::RenderGraphNodeDesc render_graph_node_desc{};
        render_graph_node_desc.draw_info = render_pass_draw_desc;
        render_graph_node_desc.render_pass_handle = render_pass_handle;

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
        return true;
    }

    bool RenderGraph::CompileRenderPassAndExecute()
    {
        m_window.RegisterTickCallback([this](unsigned long long interval)
        {
            // find final color output and copy to swapchain buffer, only debug logic
            GLTF_CHECK(m_final_color_output);
        
            if (m_tick_callback)
            {
                m_tick_callback(interval);
            }

            m_resource_allocator.WaitFrameRenderFinished();
            
            auto& command_list = m_resource_allocator.GetCommandListForRecordPassCommand();
            // Wait current frame available
            m_resource_allocator.GetCurrentSwapchain().AcquireNewFrame(m_resource_allocator.GetDevice());
            
            for (auto render_graph_node : m_render_graph_node_handles)
            {
                ExecuteRenderGraphNode(command_list, render_graph_node, interval);
            }

            if (!m_render_graph_node_handles.empty())
            {
                auto src = m_final_color_output;
                auto dst = m_resource_allocator.GetCurrentSwapchainRT().m_source;

                src->Transition(command_list, RHIResourceStateType::STATE_COPY_SOURCE);
                dst->Transition(command_list, RHIResourceStateType::STATE_COPY_DEST);
                
                RHICopyTextureInfo copy_info{};
                copy_info.copy_width = m_window.GetWidth();
                copy_info.copy_height = m_window.GetHeight();
                copy_info.dst_mip_level = 0;
                copy_info.src_mip_level = 0;
                copy_info.dst_x = 0;
                copy_info.dst_y = 0;
                RHIUtilInstanceManager::Instance().CopyTexture(command_list, *dst, *src, copy_info);
            }
                
            Present(command_list);

            // Clear all nodes at end of frame
            m_render_graph_node_handles.clear();
        });

        return true;
    }

    void RenderGraph::RegisterTextureToColorOutput(TextureHandle texture_handle)
    {
        auto texture = InternalResourceHandleTable::Instance().GetTexture(texture_handle);
        m_final_color_output = texture->m_texture;
    }

    void RenderGraph::RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle)
    {
        auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_handle);
        m_final_color_output = render_target->m_source;
    }

    void RenderGraph::RegisterTickCallback(const RenderGraphTickCallback& callback)
    {
        m_tick_callback = callback;
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
        RHIUtilInstanceManager::Instance().SetPrimitiveTopology( command_list, RHIPrimitiveTopologyType::TRIANGLELIST);

        RHIViewportDesc viewport{};
        viewport.width = render_pass->GetViewportSize().first >= 0 ? render_pass->GetViewportSize().first : m_window.GetWidth();
        viewport.height = render_pass->GetViewportSize().second >= 0 ? render_pass->GetViewportSize().second : m_window.GetHeight();
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
        for (const auto& render_target_info : render_graph_node_desc.draw_info.render_target_resources)
        {
            GLTF_CHECK(render_target_info.first != NULL_HANDLE);
            auto render_target = InternalResourceHandleTable::Instance().GetRenderTarget(render_target_info.first);
            begin_rendering_info.m_render_targets.push_back(render_target.get());

            if (render_target_info.second.need_clear)
            {
                clear_render_target = true;
                if (render_target_info.second.usage == RenderPassResourceUsage::DEPTH_STENCIL)
                {
                    begin_rendering_info.enable_depth_write = true;
                }
            }
        }

        begin_rendering_info.rendering_area_offset_x = viewport.top_left_x;
        begin_rendering_info.rendering_area_offset_y = viewport.top_left_y;
        begin_rendering_info.rendering_area_width = viewport.width;
        begin_rendering_info.rendering_area_height = viewport.height;
        begin_rendering_info.clear_render_target = clear_render_target;

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
        
        for (const auto& buffer : render_graph_node_desc.draw_info.buffer_resources)
        {
            GLTF_CHECK(buffer.second.buffer_handle != NULL_HANDLE);
            auto& root_signature_allocation = render_pass->GetRootSignatureAllocation(buffer.first);
            auto buffer_handle = buffer.second.buffer_handle;
            auto buffer_allocation = RendererInterface::InternalResourceHandleTable::Instance().GetBuffer(buffer_handle);
            auto buffer_size = buffer_allocation->m_buffer->GetBufferDesc().width;

            if (!render_pass_descriptor_resource.m_buffer_descriptors.contains(buffer.first))
            {
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
            
            if (!render_pass_descriptor_resource.m_texture_descriptors.contains(texture.first) && !render_pass_descriptor_resource.m_texture_descriptor_tables.contains(texture.first))
            {
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
            if (!render_pass_descriptor_resource.m_texture_descriptors.contains(render_target_pair.first))
            {
                std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>> descriptor_allocations;

                for (const auto& render_target : render_target_pair.second.render_target_texture)
                {
                    auto texture = InternalResourceHandleTable::Instance().GetRenderTarget(render_target)->m_source;
                    RHITextureDescriptorDesc texture_descriptor_desc{texture->GetTextureFormat(), RHIResourceDimension::TEXTURE2D, render_target_pair.second.type == TextureBindingDesc::SRV? RHIViewType::RVT_SRV :RHIViewType::RVT_UAV};
                    if (IsDepthStencilFormat(texture->GetTextureFormat()) && render_target_pair.second.type == TextureBindingDesc::SRV)
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
            }

            auto& root_signature_allocation = render_pass->GetRootSignatureAllocation(render_target_pair.first);
            if (is_texture_table)
            {
                auto descriptor_table = render_pass_descriptor_resource.m_texture_descriptor_tables.at(render_target_pair.first);
                auto& table_texture_descriptors = render_pass_descriptor_resource.m_texture_descriptor_table_source_data.at(render_target_pair.first);
                for (const auto& table_texture : table_texture_descriptors)
                {
                    table_texture->m_source->Transition(command_list, render_target_pair.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
                }
                
                render_pass->GetDescriptorUpdater().BindDescriptor(command_list, pipeline_type, root_signature_allocation, *descriptor_table, render_target_pair.second.type == TextureBindingDesc::SRV? RHIDescriptorRangeType::SRV : RHIDescriptorRangeType::UAV);
            }
            else
            {
                auto descriptor = render_pass_descriptor_resource.m_texture_descriptors.at(render_target_pair.first);
                descriptor->m_source->Transition(command_list, render_target_pair.second.type == TextureBindingDesc::SRV ? RHIResourceStateType::STATE_ALL_SHADER_RESOURCE : RHIResourceStateType::STATE_UNORDERED_ACCESS);
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
    
        RHIUtilInstanceManager::Instance().Present(m_resource_allocator.GetCurrentSwapchain(), m_resource_allocator.GetCommandQueue(), m_resource_allocator.GetCommandListForRecordPassCommand(NULL_HANDLE));
        CloseCurrentCommandListAndExecute(command_list, {}, false);
    }
}


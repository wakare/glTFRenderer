#pragma once

#include <functional>
#include <set>
#include <glm/glm/glm.hpp>

#include "Renderer.h"

class IRHITexture;
class IRHIDescriptorTable;
class MaterialBase;
class RendererInputDevice;
class IRHIBufferDescriptorAllocation;
class IRHIDescriptorManager;
class IRHIDevice;
class IRHISwapChain;
class IRHITextureDescriptorAllocation;
class IRHICommandList;
class IRHICommandQueue;
class RenderPass;
class ResourceManager;
struct RHIExecuteCommandListContext;

namespace RendererInterface
{
    class RenderWindow
    {
    public:
        typedef std::function<void(unsigned long long)> RenderWindowTickCallback;
        
        RenderWindow(const RenderWindowDesc& desc);
        RenderWindowHandle GetHandle() const;
        unsigned GetWidth() const;
        unsigned GetHeight() const;
        HWND GetHWND() const;
        void EnterWindowEventLoop();

        void RegisterTickCallback(const RenderWindowTickCallback& callback);
        const RendererInputDevice& GetInputDeviceConst() const;
        RendererInputDevice& GetInputDevice();
        
    protected:
        
        RenderWindowDesc m_desc;
        RenderWindowHandle m_handle;
        HWND m_hwnd;
        std::shared_ptr<RendererInputDevice> m_input_device;
    };

    struct BufferUploadDesc
    {
        const void* data;
        size_t size;
    };
    
    class ResourceOperator
    {
    public:
        ResourceOperator(RenderDeviceDesc device);
        unsigned            GetCurrentBackBufferIndex() const;
        
        ShaderHandle        CreateShader(const ShaderDesc& desc);
        TextureHandle       CreateTexture(const TextureDesc& desc);
        TextureHandle       CreateTexture(const TextureFileDesc& desc);
        BufferHandle        CreateBuffer(const BufferDesc& desc);
        IndexedBufferHandle CreateIndexedBuffer(const BufferDesc& desc);
        
        RenderTargetHandle  CreateRenderTarget(const RenderTargetDesc& desc);
        RenderPassHandle    CreateRenderPass(const RenderPassDesc& desc);
        RenderSceneHandle   CreateRenderScene(const RenderSceneDesc& desc);
        
        IRHIDevice&         GetDevice() const;
        IRHICommandQueue&   GetCommandQueue() const;
        IRHISwapChain&      GetCurrentSwapchain() const;
        IRHICommandList&    GetCommandListForRecordPassCommand(RenderPassHandle pass = NULL_HANDLE) const;
        IRHIDescriptorManager& GetDescriptorManager() const;
        
        IRHITextureDescriptorAllocation& GetCurrentSwapchainRT() const;

        void UploadBufferData(BufferHandle handle, const BufferUploadDesc& upload_desc);
        
    protected:
        std::shared_ptr<ResourceManager> m_resource_manager;
        std::map<RenderPassHandle, std::shared_ptr<RenderPass>> m_render_passes;
    };

    class RenderGraph
    {
    public:
        RenderGraph(ResourceOperator& allocator, RenderWindow& window);
        
        RenderGraphNodeHandle CreateRenderGraphNode(const RenderGraphNodeDesc& render_graph_node_desc);
        
        bool RegisterRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        bool RemoveRenderGraphNode(RenderGraphNodeHandle render_graph_node_handle);
        
        bool CompileRenderPassAndExecute();

        void RegisterTextureToColorOutput(TextureHandle texture_handle);
        void RegisterRenderTargetToColorOutput(RenderTargetHandle render_target_handle);

    protected:
        void ExecuteRenderGraphNode(IRHICommandList& command_list, RenderGraphNodeHandle render_graph_node_handle, unsigned long long interval);
        void CloseCurrentCommandListAndExecute(IRHICommandList& command_list, const RHIExecuteCommandListContext& context, bool wait);
        void Present(IRHICommandList& command_list);
        
        ResourceOperator& m_resource_allocator;
        RenderWindow& m_window;
        
        std::vector<RenderGraphNodeDesc> m_render_graph_nodes;
        std::set<RenderGraphNodeHandle> m_render_graph_node_handles;

        std::map<std::string, std::shared_ptr<IRHIBufferDescriptorAllocation>> m_buffer_descriptors;
        std::map<std::string, std::shared_ptr<IRHITextureDescriptorAllocation>> m_texture_descriptors;
        std::map<std::string, std::shared_ptr<IRHIDescriptorTable>> m_texture_descriptor_tables;
        std::map<std::string, std::vector<std::shared_ptr<IRHITextureDescriptorAllocation>>> m_texture_descriptor_table_source_data;

        std::shared_ptr<IRHITexture> m_final_color_output;
    };

    class RendererSceneMeshDataAccessorBase
    {
    public:
        enum class MeshDataAccessorType
        {
            VERTEX_POSITION_FLOAT3,
            VERTEX_NORMAL_FLOAT3,
            VERTEX_TANGENT_FLOAT4,
            VERTEX_TEXCOORD0_FLOAT2,
            INDEX_INT,
            INDEX_HALF,
            INSTANCE_MAT4x4,
        };

        virtual bool HasMeshData(unsigned mesh_id) const = 0;
        virtual void AccessMeshData(MeshDataAccessorType type, unsigned mesh_id, void* data, size_t element_size) = 0;
        virtual void AccessInstanceData(MeshDataAccessorType type, unsigned instance_id, unsigned mesh_id, void* data, size_t element_size) = 0;

        virtual void AccessMaterialData(const MaterialBase& material, unsigned mesh_id) = 0;
    };
    
    class RendererSceneResourceManager
    {
    public:
        RendererSceneResourceManager(ResourceOperator& allocator,const RenderSceneDesc& desc);

        bool AccessSceneData(RendererSceneMeshDataAccessorBase& data_accessor);
        
    protected:
        ResourceOperator& m_allocator;
        RenderSceneHandle m_render_scene_handle {NULL_HANDLE};
    };

}

#pragma once
#include "glTFGraphicsPassTestTriangleBase.h"
#include "RHIVertexStreamingManager.h"

class IRHITextureAllocation;
struct IndexBufferData;
struct VertexBufferData;
class IRHIIndexBufferView;
class IRHIVertexBufferView;
class RHIIndexBuffer;
class RHIVertexBuffer;

class glTFGraphicsPassTestIndexedTextureTriangle : public glTFGraphicsPassTestTriangleBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassTestIndexedTextureTriangle)

    virtual const char* PassName() override;
    virtual bool InitRenderInterface(glTFRenderResourceManager& resource_manager) override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool PreRenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupRootSignature(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;

protected:
    bool InitVertexBufferAndIndexBuffer(glTFRenderResourceManager& resource_manager);
    virtual const RHIVertexStreamingManager& GetVertexStreamingManager(glTFRenderResourceManager& resource_manager) const;
    
    std::shared_ptr<VertexBufferData> m_vertex_buffer_data;
    std::shared_ptr<IndexBufferData> m_index_buffer_data;
    
    std::shared_ptr<RHIVertexBuffer> m_vertex_buffer;
    std::shared_ptr<RHIIndexBuffer> m_index_buffer;

    std::shared_ptr<IRHIVertexBufferView> m_vertex_buffer_view;
    std::shared_ptr<IRHIIndexBufferView> m_index_buffer_view;

    RootSignatureAllocation m_sampled_texture_root_signature_allocation;
    std::shared_ptr<IRHITextureAllocation> m_sampled_texture;
    std::shared_ptr<IRHITextureDescriptorAllocation> m_sampled_texture_allocation;

    RHIVertexStreamingManager m_vertex_streaming_manager;
};

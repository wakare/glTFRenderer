#pragma once
#include "glTFGraphicsPassTestTriangleBase.h"
#include "glTFRHI/RHIVertexStreamingManager.h"

struct IndexBufferData;
struct VertexBufferData;
class IRHIIndexBufferView;
class IRHIVertexBufferView;
class IRHIIndexBuffer;
class IRHIVertexBuffer;

class glTFGraphicsPassTestIndexedTriangle : public glTFGraphicsPassTestTriangleBase
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(glTFGraphicsPassTestIndexedTriangle)

    virtual const char* PassName() override;
    virtual bool InitPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool RenderPass(glTFRenderResourceManager& resource_manager) override;
    virtual bool SetupPipelineStateObject(glTFRenderResourceManager& resource_manager) override;
    
protected:
    bool InitVertexBufferAndIndexBuffer(glTFRenderResourceManager& resource_manager);

    std::shared_ptr<VertexBufferData> m_vertex_buffer_data;
    std::shared_ptr<IndexBufferData> m_index_buffer_data;
    
    std::shared_ptr<IRHIVertexBuffer> m_vertex_buffer;
    std::shared_ptr<IRHIIndexBuffer> m_index_buffer;

    std::shared_ptr<IRHIVertexBufferView> m_vertex_buffer_view;
    std::shared_ptr<IRHIIndexBufferView> m_index_buffer_view;

    RHIVertexStreamingManager m_vertex_streaming_manager;
};

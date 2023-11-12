#pragma once
#include "IRHIDevice.h"
#include "IRHIIndexBufferView.h"
#include "IRHIResource.h"
#include "IRHIRootSignature.h"
#include "IRHIVertexBufferView.h"

struct RHIIndirectArgumentDraw
{
    unsigned VertexCountPerInstance;
    unsigned InstanceCount;
    unsigned StartVertexLocation;
    unsigned StartInstanceLocation;
};

struct RHIIndirectArgumentDrawIndexed
{
    unsigned IndexCountPerInstance;
    unsigned InstanceCount;
    unsigned StartIndexLocation;
    unsigned BaseVertexLocation;
    unsigned StartInstanceLocation;
};

struct RHIIndirectArgumentDispatch
{
    unsigned ThreadGroupCountX;
    unsigned ThreadGroupCountY;
    unsigned ThreadGroupCountZ;
};

struct RHIIndirectArgumentView
{
    RHIGPUDescriptorHandle handle;
};

struct RHIIndirectArgumentVertexBufferView
{
    RHIIndirectArgumentVertexBufferView(const IRHIVertexBufferView& vertex_buffer_view)
        : handle(vertex_buffer_view.GetGPUHandle())
        , size(vertex_buffer_view.GetSize())
        , stride(vertex_buffer_view.GetStride())
    
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned stride;
};

struct RHIIndirectArgumentIndexBufferView
{
    RHIIndirectArgumentIndexBufferView(const IRHIIndexBufferView& index_buffer_view)
        : handle(index_buffer_view.GetGPUHandle())
        , size(index_buffer_view.GetSize())
        , format(index_buffer_view.GetFormat())
    {
        
    }
    
    RHIGPUDescriptorHandle handle;
    unsigned size;
    unsigned format; 
};

enum class RHIIndirectArgType
{
    Draw,
    DrawIndexed,
    Dispatch,
    VertexBufferView,
    IndexBufferView,
    Constant,
    ConstantBufferView,
    ShaderResourceView,
    UnOrderAccessView,
    DispatchRays,
    DispatchMesh
};

struct RHIIndirectArgumentDesc
{
    RHIIndirectArgType type;

    union 
    {
        struct 
        {
            unsigned slot;    
        } vertex_buffer;

        struct 
        {
            unsigned root_parameter_index;
            unsigned dest_offset_in_32bit_value;
            unsigned num_32bit_values_to_set;
        } constant;

        struct
        {
            unsigned root_parameter_index;
        } constant_buffer_view;
        
        struct
        {
            unsigned root_parameter_index;
        } shader_resource_view;
        
        struct
        {
            unsigned root_parameter_index;
        } unordered_access_view;
    } desc;
};

struct IRHICommandSignatureDesc
{
    std::vector<RHIIndirectArgumentDesc> m_indirect_arguments;
    unsigned stride;
};

class IRHICommandSignature : public IRHIResource
{
public:
    IRHICommandSignature();
    
    virtual bool InitCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) = 0;
    
    void SetCommandSignatureDesc(const IRHICommandSignatureDesc& desc);
    
protected:
    IRHICommandSignatureDesc m_desc;
};

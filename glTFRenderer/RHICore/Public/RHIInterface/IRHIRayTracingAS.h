#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "RHIInterface/IRHICommandList.h"
#include "RHIInterface/IRHIResource.h"

class IRHIDescriptorAllocation;
class RHIVertexBuffer;
class RHIIndexBuffer;

struct RHIRayTracingGeometryDesc
{
    std::shared_ptr<RHIVertexBuffer> vertex_buffer;
    std::shared_ptr<RHIIndexBuffer> index_buffer;
    size_t vertex_count{0};
    size_t index_count{0};
    bool opaque{true};
};

struct RHIRayTracingInstanceDesc
{
    std::array<float, 12> transform{};
    uint32_t instance_id{0};
    uint32_t instance_mask{0xFF};
    uint32_t hit_group_index{0};
    uint32_t geometry_index{0};
};

struct RHIRayTracingSceneDesc
{
    std::vector<RHIRayTracingGeometryDesc> geometries;
    std::vector<RHIRayTracingInstanceDesc> instances;
};

class RHICORE_API IRHIRayTracingAS
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIRayTracingAS)
    
    virtual void SetRayTracingSceneDesc(const RHIRayTracingSceneDesc& scene_desc) = 0;
    virtual bool InitRayTracingAS(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager&
                                  memory_manager) = 0;
    virtual const IRHIDescriptorAllocation& GetTLASDescriptorSRV() const = 0;
};

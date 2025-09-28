#pragma once
#include <memory>
#include <vector>

#include "RHICommon.h"

class IRHIIndexBufferView;
class IRHIVertexBufferView;
class IRHICommandList;
class IRHIRootSignature;
class IRHICommandSignature;
class IRHIDevice;
class IRHIMemoryManager;
class IRHIBuffer;
class IRHIBufferAllocation;

__declspec(align(16)) struct MeshIndirectDrawCommand
{
    inline static std::string Name = "INDIRECT_DRAW_DATA_REGISTER_SRV_INDEX";

    MeshIndirectDrawCommand() = default;
    
    // Draw arguments
    RHIIndirectArgumentDrawIndexed draw_command_argument{};
};


class RHICORE_API RHIIndirectDrawBuilder
{
public:
    bool InitIndirectDrawBuilder(IRHIDevice& device, IRHICommandList& command_list, IRHIMemoryManager& memory_manager, const std::vector<RHIIndirectArgumentDesc>& indirect_argument_desc, unsigned command_stride, const void* data, size_t size);
    std::shared_ptr<IRHICommandSignature> BuildCommandSignature(IRHIDevice& device, IRHIRootSignature& root_signature) const;
    
    unsigned GetCommandStride() const;
    
    std::shared_ptr<IRHIBuffer> GetIndirectArgumentBuffer() const;
    std::shared_ptr<IRHIBuffer> GetCulledIndirectArgumentBuffer() const;
    std::shared_ptr<IRHIBuffer> GetCountBuffer() const;

    bool DrawIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, bool use_dynamic_count);

    RHICommandSignatureDesc GetDesc() const;
    bool GetCachedData(const char*& out_data, size_t& out_size ) const;
    unsigned GetCachedCommandCount() const; 
    
    bool ResetCountBuffer(IRHIMemoryManager& memory_manager);
    
protected:
    bool m_inited {false};
    
    RHICommandSignatureDesc m_command_signature_desc{};
    std::shared_ptr<IRHIBufferAllocation> m_indirect_argument_buffer;
    std::shared_ptr<IRHIBufferAllocation> m_culled_indirect_commands;
    std::shared_ptr<IRHIBufferAllocation> m_count_buffer;
    
    unsigned m_command_stride {0};
    
    std::unique_ptr<char[]> m_cached_data;
    size_t m_cached_data_size {0};
    size_t m_cached_command_count {0};
};

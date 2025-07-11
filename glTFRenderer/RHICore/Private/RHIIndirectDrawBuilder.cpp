#include "RHIIndirectDrawBuilder.h"

#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"
#include "RHIInterface/IRHIMemoryManager.h"

bool RHIIndirectDrawBuilder::InitIndirectDrawBuilder(IRHIDevice& device, IRHIMemoryManager& memory_manager,
    const std::vector<RHIIndirectArgumentDesc>& indirect_argument_desc, unsigned command_stride, const void* data, size_t size)
{
    GLTF_CHECK(!m_inited);
    m_inited = true;
    
    m_command_signature_desc.m_indirect_arguments = indirect_argument_desc;
    m_command_signature_desc.stride = command_stride;
    
    memory_manager.AllocateBufferMemory(
        device,
    {
        L"indirect_arguments_buffer",
        size,
        1,
        1,
        RHIBufferType::Upload,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COMMON,
        static_cast<RHIResourceUsageFlags>(RUF_INDIRECT_BUFFER | RUF_TRANSFER_DST)
        },
        m_indirect_argument_buffer);

    memory_manager.UploadBufferData(*m_indirect_argument_buffer, data, 0, size);

    memory_manager.AllocateBufferMemory(
    device, 
    {
        L"culled_indirect_arguments_buffer",
        RHIUtilInstanceManager::Instance().GetAlignmentSizeForUAVCount(size),
        1,
        1,
        RHIBufferType::Default,
        RHIDataFormat::UNKNOWN,
        RHIBufferResourceType::Buffer,
        RHIResourceStateType::STATE_COPY_DEST,
        static_cast<RHIResourceUsageFlags>(RUF_INDIRECT_BUFFER | RUF_ALLOW_UAV)
    },
    m_culled_indirect_commands
    );
    
    memory_manager.AllocateBufferMemory(
            device,
        {L"CountBuffer",
            4,
            1,
            1,
            RHIBufferType::Default,
            RHIDataFormat::R32_UINT,
            RHIBufferResourceType::Buffer,
            RHIResourceStateType::STATE_COMMON,
            static_cast<RHIResourceUsageFlags>(RUF_TRANSFER_SRC | RUF_ALLOW_UAV | RUF_INDIRECT_BUFFER),
            },
            m_count_buffer);
    
    // Cache data in host side
    m_cached_data_size = size;
    m_cached_command_count = size / command_stride;
    m_cached_data = std::make_unique<char[]>(size);
    memcpy(m_cached_data.get(), data, size);
    
    return true;
}

std::shared_ptr<IRHICommandSignature> RHIIndirectDrawBuilder::BuildCommandSignature(IRHIDevice& device,
    IRHIRootSignature& root_signature) const
{
    auto command_signature = RHIResourceFactory::CreateRHIResource<IRHICommandSignature>();
    command_signature->SetCommandSignatureDesc(m_command_signature_desc);
    if (!command_signature->InitCommandSignature(device, root_signature))
    {
        GLTF_CHECK(false);
        return nullptr;
    }

    return command_signature;
}

unsigned RHIIndirectDrawBuilder::GetCommandStride() const
{
    GLTF_CHECK(m_inited);
    return  m_command_signature_desc.stride;
}

std::shared_ptr<IRHIBuffer> RHIIndirectDrawBuilder::GetIndirectArgumentBuffer() const
{
    GLTF_CHECK(m_inited);
    return m_indirect_argument_buffer->m_buffer;
}

std::shared_ptr<IRHIBuffer> RHIIndirectDrawBuilder::GetCulledIndirectArgumentBuffer() const
{
    GLTF_CHECK(m_inited);
    return m_culled_indirect_commands->m_buffer;
}

std::shared_ptr<IRHIBuffer> RHIIndirectDrawBuilder::GetCountBuffer() const
{
    GLTF_CHECK(m_inited);
    return m_count_buffer->m_buffer;
}

bool RHIIndirectDrawBuilder::DrawIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, bool use_dynamic_count)
{
    GLTF_CHECK(m_inited);
    
    if (use_dynamic_count)
    {
        RHIUtilInstanceManager::Instance().ExecuteIndirect(
            command_list,
            command_signature,
            m_cached_command_count,
            *m_culled_indirect_commands->m_buffer,
            0,
            *m_count_buffer->m_buffer,
            0,
            GetCommandStride());
    }
    else
    {
        RHIUtilInstanceManager::Instance().ExecuteIndirect(
            command_list,
            command_signature,
            m_cached_command_count,
            *m_indirect_argument_buffer->m_buffer,
            0,
            GetCommandStride());
    }
    
    return true;
}

RHICommandSignatureDesc RHIIndirectDrawBuilder::GetDesc() const
{
    GLTF_CHECK(m_inited);
    return m_command_signature_desc;
}

bool RHIIndirectDrawBuilder::GetCachedData(const char*& out_data, size_t& out_size) const
{
    GLTF_CHECK(m_inited);
    out_data = m_cached_data.get();
    out_size = m_cached_data_size;
    
    return true;
}

unsigned RHIIndirectDrawBuilder::GetCachedCommandCount() const
{
    GLTF_CHECK(m_inited);
    return m_cached_command_count;
}

bool RHIIndirectDrawBuilder::ResetCountBuffer(IRHIMemoryManager& memory_manager)
{
    return true;
    /*
    unsigned reset_count = 0;
    return memory_manager.UploadBufferData(*m_count_buffer, &reset_count, 0, sizeof(unsigned));
    */
}
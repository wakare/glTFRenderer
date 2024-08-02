#include "IRHIIndirectDrawBuilder.h"

#include "RHIResourceFactoryImpl.hpp"
#include "RHIUtils.h"
#include "RHIInterface/IRHIMemoryManager.h"

bool IRHIIndirectDrawBuilder::InitIndirectDrawBuilder(IRHIDevice& device, IRHIMemoryManager& memory_manager,
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

    m_culled_indirect_command_count_offset = RHIUtils::Instance().GetAlignmentSizeForUAVCount(size);
    memory_manager.AllocateBufferMemory(
    device, 
    {
        L"culled_indirect_arguments_buffer",
        GetCulledIndirectArgumentBufferCountOffset() + /*count buffer*/sizeof(unsigned),
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

    // Cache data in host side
    m_cached_data_size = size;
    m_cached_command_count = size / command_stride;
    m_cached_data = std::make_unique<char[]>(size);
    memcpy(m_cached_data.get(), data, size);
    
    return true;
}

std::shared_ptr<IRHICommandSignature> IRHIIndirectDrawBuilder::BuildCommandSignature(IRHIDevice& device,
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

unsigned IRHIIndirectDrawBuilder::GetCommandStride() const
{
    GLTF_CHECK(m_inited);
    return  m_command_signature_desc.stride;
}

std::shared_ptr<IRHIBuffer> IRHIIndirectDrawBuilder::GetIndirectArgumentBuffer() const
{
    GLTF_CHECK(m_inited);
    return m_indirect_argument_buffer->m_buffer;
}

std::shared_ptr<IRHIBuffer> IRHIIndirectDrawBuilder::GetCulledIndirectArgumentBuffer() const
{
    GLTF_CHECK(m_inited);
    return m_culled_indirect_commands->m_buffer;
}

unsigned IRHIIndirectDrawBuilder::GetCulledIndirectArgumentBufferCountOffset() const
{
    GLTF_CHECK(m_inited);
    return m_culled_indirect_command_count_offset;
}

bool IRHIIndirectDrawBuilder::DrawIndirect(IRHICommandList& command_list, IRHICommandSignature& command_signature, bool use_dynamic_count)
{
    GLTF_CHECK(m_inited);
    
    if (use_dynamic_count)
    {
        RHIUtils::Instance().ExecuteIndirect(
            command_list,
            command_signature,
            m_cached_command_count,
            *m_culled_indirect_commands->m_buffer,
            0,
            *m_culled_indirect_commands->m_buffer,
            m_culled_indirect_command_count_offset);    
    }
    else
    {
        RHIUtils::Instance().ExecuteIndirect(
            command_list,
            command_signature,
            m_cached_command_count,
            *m_indirect_argument_buffer->m_buffer,
            0,
            GetCommandStride());
    }
    
    return true;
}

RHICommandSignatureDesc IRHIIndirectDrawBuilder::GetDesc() const
{
    GLTF_CHECK(m_inited);
    return m_command_signature_desc;
}

bool IRHIIndirectDrawBuilder::GetCachedData(const char*& out_data, size_t& out_size) const
{
    GLTF_CHECK(m_inited);
    out_data = m_cached_data.get();
    out_size = m_cached_data_size;
    
    return true;
}

unsigned IRHIIndirectDrawBuilder::GetCachedCommandCount() const
{
    GLTF_CHECK(m_inited);
    return m_cached_command_count;
}

#include "IRHITexture.h"

#include "RHIUtils.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

const RHITextureDesc& IRHITexture::GetTextureDesc() const
{
    return m_texture_desc;
}

RHIDataFormat IRHITexture::GetTextureFormat() const
{
    return GetTextureDesc().GetDataFormat();
}

bool IRHITexture::Transition(IRHICommandList& command_list, RHIResourceStateType new_state)
{
    if (m_current_state == new_state)
    {
        return true;
    }
    
    RETURN_IF_FALSE(RHIUtilInstanceManager::Instance().AddTextureBarrierToCommandList(command_list, *this, m_current_state, new_state))

    m_current_state = new_state;
    return true;
}

RHIResourceStateType IRHITexture::GetState() const
{
    return m_current_state;
}

bool IRHITexture::CanReadBack() const
{
    return m_texture_desc.HasUsage(RHIResourceUsageFlags::RUF_READBACK);
}

bool IRHITexture::Release(IRHIMemoryManager& memory_manager)
{
    return true;
}

void IRHITexture::SetCopyReq(RHIMipMapCopyRequirements copy_requirements)
{
    m_copy_requirements = copy_requirements;
}

RHIMipMapCopyRequirements IRHITexture::GetCopyReq() const
{
    return m_copy_requirements;
}

unsigned IRHITexture::GetMipCount() const
{
    return m_copy_requirements.row_byte_size.size();
}

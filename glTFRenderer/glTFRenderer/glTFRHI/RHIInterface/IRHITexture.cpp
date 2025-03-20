#include "IRHITexture.h"

#include "glTFRenderPass/glTFRenderResourceManager.h"
#include "glTFRHI/RHIUtils.h"
#include "SceneFileLoader/glTFImageIOUtil.h"

bool IRHITexture::Transition(IRHICommandList& command_list, RHIResourceStateType new_state)
{
    if (m_current_state == new_state)
    {
        return true;
    }
    
    RETURN_IF_FALSE(RHIUtils::Instance().AddTextureBarrierToCommandList(command_list, *this, m_current_state, new_state))

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

bool IRHITexture::Release(glTFRenderResourceManager&)
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

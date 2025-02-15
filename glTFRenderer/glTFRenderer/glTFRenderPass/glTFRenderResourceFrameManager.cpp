#include "glTFRenderResourceFrameManager.h"

glTFRenderResourceFrameManager::glTFRenderResourceFrameManager()
{
}

glTFRenderResourceUtils::GBufferOutput& glTFRenderResourceFrameManager::GetGBufferForInit()
{
    return m_GBuffer;
}

const glTFRenderResourceUtils::GBufferOutput& glTFRenderResourceFrameManager::GetGBufferForRendering() const
{
    GLTF_CHECK(m_GBuffer.m_albedo_output && m_GBuffer.m_normal_output && m_GBuffer.m_depth_output);
    return m_GBuffer;
}

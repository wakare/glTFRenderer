#include "RendererModuleLighting.h"
#include <cmath>
#include <glm/glm/gtx/norm.hpp>

namespace
{
    constexpr float LIGHT_INFO_FLOAT_EPSILON = 1.0e-6f;
    constexpr float LIGHT_INFO_VECTOR_EPSILON_SQ = LIGHT_INFO_FLOAT_EPSILON * LIGHT_INFO_FLOAT_EPSILON;

    bool IsLightInfoEquivalent(const LightInfo& lhs, const LightInfo& rhs)
    {
        return lhs.type == rhs.type &&
            std::abs(lhs.radius - rhs.radius) <= LIGHT_INFO_FLOAT_EPSILON &&
            glm::length2(lhs.position - rhs.position) <= LIGHT_INFO_VECTOR_EPSILON_SQ &&
            glm::length2(lhs.intensity - rhs.intensity) <= LIGHT_INFO_VECTOR_EPSILON_SQ;
    }
}

RendererModuleLighting::RendererModuleLighting(RendererInterface::ResourceOperator& resource_operator)
{
    RendererInterface::BufferDesc light_buffer_desc{};
    light_buffer_desc.name = "g_lightInfos";
    light_buffer_desc.size = sizeof(LightInfo) * MAX_LIGHT_COUNT;
    light_buffer_desc.type = RendererInterface::DEFAULT;
    light_buffer_desc.usage = RendererInterface::USAGE_SRV;
    m_light_buffer_handles = resource_operator.CreateFrameBufferedBuffers(light_buffer_desc, "g_lightInfos");

    RendererInterface::BufferDesc light_count_buffer_desc{};
    light_count_buffer_desc.name = "LightInfoConstantBuffer";
    light_count_buffer_desc.size = sizeof(int);
    light_count_buffer_desc.type = RendererInterface::DEFAULT;
    light_count_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_light_count_buffer_handles = resource_operator.CreateFrameBufferedBuffers(light_count_buffer_desc, "LightInfoConstantBuffer");
}

unsigned RendererModuleLighting::AddLightInfo(const LightInfo& info)
{
    unsigned index = m_light_infos.size();
    m_light_infos.push_back(info);

    m_need_upload_light_infos = true;

    return index;
}

const std::vector<LightInfo>& RendererModuleLighting::GetLightInfos() const
{
    return m_light_infos;
}

bool RendererModuleLighting::ContainsLight(unsigned index) const
{
    return m_light_infos.size() > index;
}

bool RendererModuleLighting::UpdateLightInfo(unsigned index, const LightInfo& info)
{
    GLTF_CHECK(ContainsLight(index));
    if (IsLightInfoEquivalent(m_light_infos[index], info))
    {
        return true;
    }

    m_light_infos[index] = info;
    
    m_need_upload_light_infos = true;
    
    return true;
}

bool RendererModuleLighting::FinalizeModule(RendererInterface::ResourceOperator& resource_operator)
{
    UploadAllLightInfos(resource_operator);
    
    return true;
}

bool RendererModuleLighting::BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc)
{
    GLTF_CHECK(!m_light_buffer_handles.empty());
    GLTF_CHECK(!m_light_count_buffer_handles.empty());

    RendererInterface::BufferBindingDesc light_buffer_binding_desc{};
    light_buffer_binding_desc.binding_type = RendererInterface::BufferBindingDesc::SRV;
    light_buffer_binding_desc.buffer_handle = m_light_buffer_handles.front();
    light_buffer_binding_desc.stride = sizeof(LightInfo);
    light_buffer_binding_desc.is_structured_buffer = true;
    light_buffer_binding_desc.count = m_light_infos.size();
    out_draw_desc.buffer_resources["g_lightInfos"] = light_buffer_binding_desc;

    RendererInterface::BufferBindingDesc light_count_binding_desc{};
    light_count_binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
    light_count_binding_desc.buffer_handle = m_light_count_buffer_handles.front();
    out_draw_desc.buffer_resources["LightInfoConstantBuffer"] = light_count_binding_desc;
    
    return true;
}

bool RendererModuleLighting::Tick(RendererInterface::ResourceOperator& resource_operator, unsigned long long interval)
{
    RETURN_IF_FALSE(RendererModuleBase::Tick(resource_operator, interval))

    UploadAllLightInfos(resource_operator);
    
    return true;
}

void RendererModuleLighting::UploadAllLightInfos(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_light_infos)
    {
        return;
    }
    
    if (!m_light_infos.empty())
    {
        RendererInterface::BufferUploadDesc light_buffer_upload_desc{};
        light_buffer_upload_desc.data = m_light_infos.data();
        light_buffer_upload_desc.size = m_light_infos.size() * sizeof(LightInfo);
        resource_operator.UploadFrameBufferedBufferData(m_light_buffer_handles, light_buffer_upload_desc);

        RendererInterface::BufferUploadDesc light_count_upload_desc{};
        unsigned int light_count = m_light_infos.size();
        light_count_upload_desc.data = &light_count;
        light_count_upload_desc.size = sizeof(unsigned);
        resource_operator.UploadFrameBufferedBufferData(m_light_count_buffer_handles, light_count_upload_desc);
    }
    
    m_need_upload_light_infos = false;
}

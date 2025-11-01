#pragma once
#include "RendererInterface.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/gtx/compatibility.hpp>

enum LightType
{
    Directional = 0,
    Point       = 1,
};

struct LightInfo
{
    glm::float3 position;
    float radius;
    
    glm::float3 intensity;
    LightType type;
};

class RendererModuleLighting : public RendererInterface::RendererModuleBase
{
public:
    enum
    {
        MAX_LIGHT_COUNT = 16,
    };
    
    RendererModuleLighting(RendererInterface::ResourceOperator& resource_operator);

    unsigned AddLightInfo(const LightInfo& info);
    const std::vector<LightInfo>& GetLightInfos() const;
    bool ContainsLight(unsigned index) const;
    bool UpdateLightInfo(unsigned index, const LightInfo& info);
    
    virtual bool FinalizeModule(RendererInterface::ResourceOperator& resource_operator) override;
    virtual bool BindDrawCommands(RendererInterface::RenderPassDrawDesc& out_draw_desc) override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, unsigned long long interval) override;
    
protected:
    void UploadAllLightInfos(RendererInterface::ResourceOperator& resource_operator);
    
    RendererInterface::BufferHandle m_light_buffer;
    RendererInterface::BufferHandle m_light_count_buffer;
    
    std::vector<LightInfo> m_light_infos;
    bool m_need_upload_light_infos {false};
};

#pragma once
#include <glm/glm/gtx/compatibility.hpp>

#include "DemoBase.h"
#include "RendererCommon.h"

class DemoTriangleApp : public DemoBase
{
public:
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(DemoTriangleApp)

protected:
    struct TriangleStateSnapshot final : NonRenderStateSnapshot
    {
        glm::float4 color{1.0f, 0.0f, 0.0f, 1.0f};
    };

    virtual bool InitInternal(const std::vector<std::string>& arguments) override;
    virtual void TickFrameInternal(unsigned long long time_interval) override;
    virtual std::shared_ptr<NonRenderStateSnapshot> CaptureNonRenderStateSnapshot() const override;
    virtual bool ApplyNonRenderStateSnapshot(const std::shared_ptr<NonRenderStateSnapshot>& snapshot) override;
    virtual bool SerializeNonRenderStateSnapshotToJson(
        const std::shared_ptr<NonRenderStateSnapshot>& snapshot,
        nlohmann::json& out_snapshot_json) const override;
    virtual std::shared_ptr<NonRenderStateSnapshot> DeserializeNonRenderStateSnapshotFromJson(
        const nlohmann::json& snapshot_json,
        std::string& out_error) const override;
    virtual const char* GetSnapshotTypeName() const override { return "DemoTriangleApp"; }
    
    glm::float4 m_color;
    RendererInterface::BufferHandle m_color_buffer_handle;
};

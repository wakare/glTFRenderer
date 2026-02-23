#pragma once
#include "RendererSystemBase.h"
#include "RendererSystemFrostedGlass.h"

class RendererSystemToneMap : public RendererSystemBase
{
public:
    explicit RendererSystemToneMap(std::shared_ptr<RendererSystemFrostedGlass> frosted);

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Tone Map"; }
    virtual void DrawDebugUI() override;

protected:
    struct ToneMapGlobalParams
    {
        float exposure{1.0f};
        float gamma{2.2f};
        unsigned tone_map_mode{1}; // 0: Reinhard, 1: ACES
        float pad0{0.0f};
    };

    void UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator);

    std::shared_ptr<RendererSystemFrostedGlass> m_frosted;

    RendererInterface::RenderGraphNodeHandle m_tone_map_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_tone_map_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_tone_map_global_params_handle{NULL_HANDLE};

    ToneMapGlobalParams m_global_params{};
    bool m_need_upload_params{true};
};

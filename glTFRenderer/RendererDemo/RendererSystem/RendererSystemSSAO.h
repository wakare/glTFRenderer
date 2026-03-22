#pragma once

#include "RendererSystemBase.h"
#include "RenderPassSetupBuilder.h"
#include "RendererModule/RendererModuleCamera.h"

#include <memory>

class RendererSystemSceneRenderer;

class RendererSystemSSAO : public RendererSystemBase
{
public:
    struct SSAOOutputs
    {
        RendererInterface::RenderGraphNodeHandle raw_node{NULL_HANDLE};
        RendererInterface::RenderGraphNodeHandle blur_node{NULL_HANDLE};
        RendererInterface::RenderTargetHandle raw_output{NULL_HANDLE};
        RendererInterface::RenderTargetHandle output{NULL_HANDLE};

        bool HasInit() const
        {
            return blur_node != NULL_HANDLE && output != NULL_HANDLE;
        }
    };

    struct SSAOGlobalParams
    {
        float radius_world{0.65f};
        float intensity{1.35f};
        float power{1.50f};
        float bias{0.02f};
        float thickness{0.30f};
        float sample_distribution_power{2.00f};
        float blur_depth_reject{10.0f};
        float blur_normal_reject{36.0f};
        unsigned sample_count{16u};
        unsigned blur_radius{2u};
        unsigned enabled{1u};
        unsigned debug_output_mode{0u}; // 0: AO, 1: valid_sample_count / sample_count
    };
    static_assert(sizeof(SSAOGlobalParams) == 48, "SSAOGlobalParams must match HLSL cbuffer layout.");

    explicit RendererSystemSSAO(std::shared_ptr<RendererSystemSceneRenderer> scene);

    RendererInterface::RenderTargetHandle GetOutput() const;
    RendererInterface::RenderGraphNodeHandle GetOutputNode() const;
    SSAOOutputs GetOutputs() const;
    const SSAOGlobalParams& GetGlobalParams() const;
    void SetGlobalParams(const SSAOGlobalParams& global_params);

    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "SSAO"; }
    virtual void DrawDebugUI() override;

protected:
    struct SSAOExecutionPlan
    {
        std::shared_ptr<RendererModuleCamera> camera_module{};
        RenderFeature::ComputeExecutionPlan compute_plan{};
    };

    void CreateOutputs(RendererInterface::ResourceOperator& resource_operator);
    SSAOExecutionPlan BuildExecutionPlan() const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildSSAOPassSetupInfo(const SSAOExecutionPlan& execution_plan) const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildBlurHorizontalPassSetupInfo(const SSAOExecutionPlan& execution_plan) const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildBlurVerticalPassSetupInfo(const SSAOExecutionPlan& execution_plan) const;
    static void ClampGlobalParams(SSAOGlobalParams& global_params);
    bool ShouldRunBlurPasses() const;
    void UploadGlobalParams(RendererInterface::ResourceOperator& resource_operator);

    std::shared_ptr<RendererSystemSceneRenderer> m_scene;

    RendererInterface::RenderGraphNodeHandle m_ssao_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_horizontal_pass_node{NULL_HANDLE};
    RendererInterface::RenderGraphNodeHandle m_blur_pass_node{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_ssao_raw_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_ssao_blur_temp_output{NULL_HANDLE};
    RendererInterface::RenderTargetHandle m_ssao_output{NULL_HANDLE};
    RendererInterface::BufferHandle m_ssao_global_params_handle{NULL_HANDLE};

    SSAOGlobalParams m_global_params{};
    bool m_need_upload_params{true};
};

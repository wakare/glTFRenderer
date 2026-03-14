#pragma once
#include "RendererSystemBase.h"
#include "RenderPassSetupBuilder.h"
#include "RendererSystemSceneRenderer.h"
#include "RendererModule/RendererModuleLighting.h"
#include "RendererModule/RendererModuleSceneMesh.h"
#include <optional>
#include <utility>
#include <vector>

class RendererSceneAABB;

class RendererSystemLighting : public RendererSystemBase
{
public:
    struct LightingOutputs
    {
        RendererInterface::RenderGraphNodeHandle node{NULL_HANDLE};
        RendererInterface::RenderTargetHandle output{NULL_HANDLE};

        bool HasInit() const
        {
            return node != NULL_HANDLE;
        }
    };

    RendererSystemLighting(RendererInterface::ResourceOperator& resource_operator, std::shared_ptr<RendererSystemSceneRenderer> scene);
    
    unsigned AddLight(const LightInfo& light_info);
    bool UpdateLight(unsigned index, const LightInfo& light_info);
    bool GetDominantDirectionalLight(glm::fvec3& out_direction, float& out_luminance) const;
    bool CastShadow() const;
    void SetCastShadow(bool cast_shadow);
    const RendererInterface::RenderStateDesc& GetDirectionalShadowRenderState() const;
    bool SetDirectionalShadowRenderState(const RendererInterface::RenderStateDesc& render_state);
    bool SetDirectionalShadowDepthBias(const RendererInterface::DepthBiasDesc& depth_bias);
    LightingOutputs GetOutputs() const;
    RendererInterface::RenderTargetHandle GetLightingOutput() const;
    
    virtual bool Init(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph) override;
    virtual bool HasInit() const override;
    virtual bool Tick(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph, unsigned long long interval) override;
    virtual void ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator) override;
    virtual void OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height) override;
    virtual const char* GetSystemName() const override { return "Lighting"; }
    virtual void DrawDebugUI() override;
    
protected:
    static RendererInterface::RenderStateDesc CreateDefaultDirectionalShadowRenderState();

    struct LightingExecutionPlan
    {
        std::shared_ptr<RendererModuleCamera> camera_module{};
        RenderFeature::ComputeExecutionPlan compute_plan{};
    };

    struct ShadowMapInfo
    {
        glm::fmat4x4 view_matrix{1.0f};
        glm::fmat4x4 projection_matrix{1.0f};
        unsigned shadowmap_size[2];
        unsigned vsm_texture_id;
        unsigned pad;    
    };
    
    struct ShadowPassResource
    {
        static bool CalcDirectionalLightShadowMatrix(const LightInfo& directional_light_info, const RendererSceneAABB& scene_bounds, float ndc_min_x, float ndc_min_y, float ndc_width, float ndc_height, unsigned
                                                           shadowmap_width, unsigned shadowmap_height, ViewBuffer& out_view_buffer, ShadowMapInfo& out_shadow_info);

        bool HasInit() const;
        bool QueueRenderStateUpdate(RendererInterface::RenderGraph& graph, const RendererInterface::RenderStateDesc& render_state) const;
        RendererInterface::RenderTargetHandle SyncFrameBindings(RendererInterface::ResourceOperator& resource_operator, RendererInterface::RenderGraph& graph);
        void Register(RendererInterface::RenderGraph& graph) const;

        RendererInterface::RenderGraphNodeHandle m_shadow_pass_node {NULL_HANDLE};
        std::vector<RendererInterface::RenderTargetHandle> m_shadow_maps;
        RendererInterface::RenderTargetHandle m_bound_shadow_map {NULL_HANDLE};
        ViewBuffer m_shadow_map_view_buffer{};
        std::vector<ViewBuffer> m_shadow_map_view_buffers;
        ShadowMapInfo m_shadow_map_info{};
        std::vector<RendererInterface::BufferHandle> m_shadow_map_buffer_handles;
    };

    struct LightingPassRuntimeState
    {
        RendererInterface::RenderGraphNodeHandle node{NULL_HANDLE};
        RendererInterface::RenderTargetHandle output{NULL_HANDLE};
        std::vector<RendererInterface::BufferHandle> shadow_infos_handles;
        std::size_t light_topology_signature{0};

        void Reset();
        bool HasInit() const;
    };

    struct DirectionalShadowRuntimeState
    {
        void Reset();
        bool HasShadowPasses() const;
        size_t GetShadowPassCount() const;
        void CreateFallbackShadowMap(RendererInterface::ResourceOperator& resource_operator);
        ShadowPassResource& GetOrCreate(unsigned light_index);
        std::map<unsigned, ShadowPassResource>& GetResources();
        const std::map<unsigned, ShadowPassResource>& GetResources() const;
        bool QueueRenderStateUpdates(RendererInterface::RenderGraph& graph, const RendererInterface::RenderStateDesc& render_state) const;
        void SyncFallbackShadowMap(RendererInterface::ResourceOperator& resource_operator);
        std::vector<RendererInterface::RenderTargetHandle> SyncAndRegisterShadowPasses(
            RendererInterface::ResourceOperator& resource_operator,
            RendererInterface::RenderGraph& graph,
            const std::vector<LightInfo>& lights);
        void CollectLightIndexedShadowMaps(
            const std::vector<LightInfo>& lights,
            std::vector<RendererInterface::RenderTargetHandle>& out_shadow_maps) const;
        void CollectFallbackLightIndexedShadowMaps(
            const std::vector<LightInfo>& lights,
            std::vector<RendererInterface::RenderTargetHandle>& out_shadow_maps) const;
        void CollectLightIndexedShadowMapInfos(
            const std::vector<LightInfo>& lights,
            std::vector<ShadowMapInfo>& out_shadowmap_infos) const;
        void CollectDependencyNodes(std::vector<RendererInterface::RenderGraphNodeHandle>& out_dependency_nodes) const;

    private:
        std::map<unsigned, ShadowPassResource> m_resources{};
        std::vector<RendererInterface::RenderTargetHandle> m_fallback_shadow_maps{};
        RendererInterface::RenderTargetHandle m_bound_fallback_shadow_map{NULL_HANDLE};
    };

    bool QueuePendingDirectionalShadowRenderStateUpdate(RendererInterface::RenderGraph& graph);
    bool SyncLightingTopology(
        RendererInterface::ResourceOperator& resource_operator,
        RendererInterface::RenderGraph& graph,
        const LightingExecutionPlan& execution_plan);
    void UpdateDirectionalShadowResources(RendererInterface::ResourceOperator& resource_operator);
    void CreateLightingOutput(RendererInterface::ResourceOperator& resource_operator);
    void CreateLightingPassShadowInfoBuffers(RendererInterface::ResourceOperator& resource_operator);
    ShadowPassResource& CreateDirectionalShadowPassResource(
        RendererInterface::ResourceOperator& resource_operator,
        RendererInterface::RenderGraph& graph,
        unsigned light_index,
        const LightInfo& light_info);
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildDirectionalShadowPassSetupInfo(
        const ShadowPassResource& shadow_pass_resource,
        unsigned light_index) const;
    RendererInterface::RenderGraph::RenderPassSetupInfo BuildLightingPassSetupInfo(
        const LightingExecutionPlan& execution_plan) const;
    LightingExecutionPlan BuildLightingExecutionPlan() const;

    bool m_cast_shadow {true};
    
    std::shared_ptr<RendererSystemSceneRenderer> m_scene;
    std::shared_ptr<RendererModuleLighting> m_lighting_module;
    RendererInterface::RenderStateDesc m_directional_shadow_render_state{};
    std::optional<RendererInterface::RenderStateDesc> m_pending_directional_shadow_render_state{};

    DirectionalShadowRuntimeState m_directional_shadow_state{};
    LightingPassRuntimeState m_lighting_pass_state{};
};

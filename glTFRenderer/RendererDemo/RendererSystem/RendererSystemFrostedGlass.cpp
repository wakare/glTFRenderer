#include "RendererSystemFrostedGlass.h"
#include "RenderPassSetupBuilder.h"
#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>
#include <utility>

namespace
{
    constexpr unsigned BLUR_SOURCE_MODE_LEGACY_PYRAMID = 0;
    constexpr unsigned BLUR_SOURCE_MODE_SHARED_MIP = 1;
    constexpr unsigned BLUR_SOURCE_MODE_SHARED_DUAL = 2;
    constexpr unsigned MULTILAYER_MODE_SINGLE = 0;
    constexpr unsigned MULTILAYER_MODE_AUTO = 1;
    constexpr unsigned MULTILAYER_MODE_FORCE = 2;
    constexpr unsigned MULTILAYER_AUTO_OVER_BUDGET_TRIGGER_FRAMES = 6;
    constexpr unsigned MULTILAYER_AUTO_COOLDOWN_FRAMES = 180;
}

RendererSystemFrostedGlass::RendererSystemFrostedGlass(std::shared_ptr<RendererSystemSceneRenderer> scene,
                                                       std::shared_ptr<RendererSystemLighting> lighting)
    : m_scene(std::move(scene))
    , m_lighting(std::move(lighting))
{
}

RendererSystemFrostedGlass::BlurPyramidBundleView RendererSystemFrostedGlass::GetPrimaryBlurPyramidView() const
{
    BlurPyramidBundleView bundle{};
    bundle.levels = m_primary_blur_pyramid.levels;
    return bundle;
}

RendererSystemFrostedGlass::BlurPyramidBundleView RendererSystemFrostedGlass::GetMultilayerBlurPyramidView() const
{
    BlurPyramidBundleView bundle{};
    bundle.levels = m_multilayer_blur_pyramid.levels;
    return bundle;
}

RendererSystemFrostedGlass::CompositePassBundleView RendererSystemFrostedGlass::GetLegacyCompositePassBundleView() const
{
    return m_legacy_composite_passes;
}

RendererSystemFrostedGlass::CompositePassBundleView RendererSystemFrostedGlass::GetSharedMipCompositePassBundleView() const
{
    return m_shared_mip_composite_passes;
}

unsigned RendererSystemFrostedGlass::AddPanel(const FrostedGlassPanelDesc& panel_desc)
{
    GLTF_CHECK(m_panel_descs.size() < MAX_PANEL_COUNT);
    const unsigned index = static_cast<unsigned>(m_panel_descs.size());
    m_panel_descs.push_back(panel_desc);
    PanelRuntimeState runtime_state{};
    runtime_state.state_weights.fill(0.0f);
    runtime_state.target_state = panel_desc.interaction_state;
    runtime_state.state_weights[ToInteractionStateIndex(panel_desc.interaction_state)] = 1.0f;
    m_panel_runtime_states.push_back(runtime_state);
    m_need_upload_panels = true;
    m_need_upload_global_params = true;
    return index;
}

bool RendererSystemFrostedGlass::UpdatePanel(unsigned index, const FrostedGlassPanelDesc& panel_desc)
{
    GLTF_CHECK(ContainsPanel(index));
    m_panel_descs[index] = panel_desc;
    if (index < m_panel_runtime_states.size())
    {
        m_panel_runtime_states[index].target_state = panel_desc.interaction_state;
    }
    m_need_upload_panels = true;
    return true;
}

unsigned RendererSystemFrostedGlass::RegisterExternalPanelProducer(ExternalPanelProducer producer)
{
    GLTF_CHECK(static_cast<bool>(producer));
    ExternalPanelProducerEntry producer_entry{};
    producer_entry.producer_id = m_next_external_panel_producer_id++;
    producer_entry.producer = std::move(producer);
    const unsigned producer_id = producer_entry.producer_id;
    m_external_panel_producers.push_back(std::move(producer_entry));
    m_need_upload_panels = true;
    return producer_id;
}

bool RendererSystemFrostedGlass::UnregisterExternalPanelProducer(unsigned producer_id)
{
    const auto producer_itr = std::find_if(
        m_external_panel_producers.begin(),
        m_external_panel_producers.end(),
        [producer_id](const ExternalPanelProducerEntry& producer_entry)
        {
            return producer_entry.producer_id == producer_id;
        });
    if (producer_itr == m_external_panel_producers.end())
    {
        return false;
    }
    m_external_panel_producers.erase(producer_itr);
    if (m_external_panel_producers.empty())
    {
        m_producer_world_space_panel_descs.clear();
        m_producer_overlay_panel_descs.clear();
        m_debug_override_producer_world.panel_descs.clear();
        m_debug_override_producer_world.enabled.clear();
        m_debug_override_producer_overlay.panel_descs.clear();
        m_debug_override_producer_overlay.enabled.clear();
    }
    m_need_upload_panels = true;
    return true;
}

void RendererSystemFrostedGlass::ClearExternalPanelProducers()
{
    if (m_external_panel_producers.empty())
    {
        return;
    }
    m_external_panel_producers.clear();
    m_producer_world_space_panel_descs.clear();
    m_producer_overlay_panel_descs.clear();
    m_debug_override_producer_world.panel_descs.clear();
    m_debug_override_producer_world.enabled.clear();
    m_debug_override_producer_overlay.panel_descs.clear();
    m_debug_override_producer_overlay.enabled.clear();
    m_need_upload_panels = true;
}

void RendererSystemFrostedGlass::SetExternalWorldSpacePanels(const std::vector<FrostedGlassPanelDesc>& panel_descs)
{
    m_external_world_space_panel_descs = panel_descs;
    for (auto& panel_desc : m_external_world_space_panel_descs)
    {
        panel_desc.world_space_mode = 1u;
    }
    m_need_upload_panels = true;
}

void RendererSystemFrostedGlass::SetExternalOverlayPanels(const std::vector<FrostedGlassPanelDesc>& panel_descs)
{
    m_external_overlay_panel_descs = panel_descs;
    for (auto& panel_desc : m_external_overlay_panel_descs)
    {
        panel_desc.world_space_mode = 0u;
        panel_desc.depth_policy = PanelDepthPolicy::Overlay;
    }
    m_need_upload_panels = true;
}

void RendererSystemFrostedGlass::ClearExternalPanels()
{
    if (m_external_world_space_panel_descs.empty() && m_external_overlay_panel_descs.empty())
    {
        return;
    }
    m_external_world_space_panel_descs.clear();
    m_external_overlay_panel_descs.clear();
    m_debug_override_manual_world.panel_descs.clear();
    m_debug_override_manual_world.enabled.clear();
    m_debug_override_manual_overlay.panel_descs.clear();
    m_debug_override_manual_overlay.enabled.clear();
    m_need_upload_panels = true;
}

void RendererSystemFrostedGlass::EnsureDebugPanelOverrideCapacity(DebugPanelOverrideBucket& bucket, unsigned panel_count)
{
    if (bucket.panel_descs.size() < panel_count)
    {
        bucket.panel_descs.resize(panel_count);
    }
    if (bucket.enabled.size() < panel_count)
    {
        bucket.enabled.resize(panel_count, 0u);
    }
}

void RendererSystemFrostedGlass::ApplyDebugPanelOverrides(std::vector<FrostedGlassPanelDesc>& panel_descs, DebugPanelOverrideBucket& bucket)
{
    const size_t override_count = (std::min)(panel_descs.size(), bucket.enabled.size());
    if (override_count == 0)
    {
        return;
    }
    for (size_t panel_index = 0; panel_index < override_count; ++panel_index)
    {
        if (bucket.enabled[panel_index] == 0u || panel_index >= bucket.panel_descs.size())
        {
            continue;
        }
        panel_descs[panel_index] = bucket.panel_descs[panel_index];
    }
}

void RendererSystemFrostedGlass::SaveDebugPanelOverride(DebugPanelSource source,
                                                        unsigned local_index,
                                                        const FrostedGlassPanelDesc& panel_desc)
{
    if (source == DebugPanelSource::Internal)
    {
        return;
    }

    DebugPanelOverrideBucket* override_bucket = nullptr;
    switch (source)
    {
    case DebugPanelSource::ProducerWorld:
        override_bucket = &m_debug_override_producer_world;
        break;
    case DebugPanelSource::ProducerOverlay:
        override_bucket = &m_debug_override_producer_overlay;
        break;
    case DebugPanelSource::ManualWorld:
        override_bucket = &m_debug_override_manual_world;
        break;
    case DebugPanelSource::ManualOverlay:
        override_bucket = &m_debug_override_manual_overlay;
        break;
    default:
        break;
    }

    if (override_bucket == nullptr)
    {
        return;
    }

    EnsureDebugPanelOverrideCapacity(*override_bucket, local_index + 1u);
    override_bucket->panel_descs[local_index] = panel_desc;
    override_bucket->enabled[local_index] = 1u;
}

void RendererSystemFrostedGlass::ApplyExternalPanelDebugOverrides()
{
    ApplyDebugPanelOverrides(m_producer_world_space_panel_descs, m_debug_override_producer_world);
    ApplyDebugPanelOverrides(m_producer_overlay_panel_descs, m_debug_override_producer_overlay);
    ApplyDebugPanelOverrides(m_external_world_space_panel_descs, m_debug_override_manual_world);
    ApplyDebugPanelOverrides(m_external_overlay_panel_descs, m_debug_override_manual_overlay);
}

bool RendererSystemFrostedGlass::ContainsPanel(unsigned index) const
{
    return index < m_panel_descs.size();
}

bool RendererSystemFrostedGlass::SetPanelInteractionState(unsigned index, PanelInteractionState interaction_state)
{
    GLTF_CHECK(ContainsPanel(index));
    GLTF_CHECK(index < m_panel_runtime_states.size());
    if (m_panel_descs[index].interaction_state == interaction_state &&
        m_panel_runtime_states[index].target_state == interaction_state)
    {
        return true;
    }
    m_panel_descs[index].interaction_state = interaction_state;
    m_panel_runtime_states[index].target_state = interaction_state;
    m_need_upload_panels = true;
    return true;
}

RendererSystemFrostedGlass::PanelInteractionState RendererSystemFrostedGlass::GetPanelInteractionState(unsigned index) const
{
    GLTF_CHECK(ContainsPanel(index));
    GLTF_CHECK(index < m_panel_runtime_states.size());
    return m_panel_runtime_states[index].target_state;
}

void RendererSystemFrostedGlass::SetBlurSourceMode(BlurSourceMode mode)
{
    const int clamped_mode = (std::max)(0, (std::min)(static_cast<int>(mode), 2));
    const BlurSourceMode next_mode = static_cast<BlurSourceMode>(clamped_mode);
    if (m_blur_source_mode == next_mode)
    {
        return;
    }

    m_blur_source_mode = next_mode;
    m_temporal_history_state.force_reset = true;
    m_temporal_history_state.valid = false;
    m_global_params.temporal_history_valid = 0;
    m_need_upload_global_params = true;
}

void RendererSystemFrostedGlass::SetFullFogMode(bool enable)
{
    (void)enable;
}

void RendererSystemFrostedGlass::ForceResetTemporalHistory()
{
    m_temporal_history_state.ResetRuntime();
    m_global_params.temporal_history_valid = 0;
    m_need_upload_global_params = true;
}

void RendererSystemFrostedGlass::ResetInitRuntimeState()
{
    m_temporal_history_state.ResetRuntime();
    m_global_params.temporal_history_valid = 0;
    m_global_params.blur_source_mode = static_cast<unsigned>(m_blur_source_mode);
    m_multilayer_runtime_enabled = m_global_params.multilayer_mode != MULTILAYER_MODE_SINGLE;
    m_multilayer_over_budget_streak = 0;
    m_multilayer_cooldown_frames = 0;
    m_global_params.multilayer_runtime_enabled = m_multilayer_runtime_enabled ? 1u : 0u;
    m_need_upload_global_params = true;
    m_static_pass_setup_state.Reset();
    m_dispatch_cache_state.Reset();
}

RendererSystemFrostedGlass::InitDimensions RendererSystemFrostedGlass::BuildInitDimensions(
    RendererInterface::ResourceOperator& resource_operator) const
{
    InitDimensions dimensions{};
    const RenderFeature::FrameDimensions frame_dimensions =
        RenderFeature::FrameDimensions::FromResourceOperator(resource_operator);
    const RenderFeature::FrameDimensions half_dimensions = frame_dimensions.Downsampled(2u);
    const RenderFeature::FrameDimensions quarter_dimensions = frame_dimensions.Downsampled(4u);
    const RenderFeature::FrameDimensions eighth_dimensions = frame_dimensions.Downsampled(8u);
    const RenderFeature::FrameDimensions sixteenth_dimensions = frame_dimensions.Downsampled(16u);
    const RenderFeature::FrameDimensions thirtysecond_dimensions = frame_dimensions.Downsampled(32u);
    dimensions.width = frame_dimensions.width;
    dimensions.height = frame_dimensions.height;
    dimensions.half_width = half_dimensions.width;
    dimensions.half_height = half_dimensions.height;
    dimensions.quarter_width = quarter_dimensions.width;
    dimensions.quarter_height = quarter_dimensions.height;
    dimensions.eighth_width = eighth_dimensions.width;
    dimensions.eighth_height = eighth_dimensions.height;
    dimensions.sixteenth_width = sixteenth_dimensions.width;
    dimensions.sixteenth_height = sixteenth_dimensions.height;
    dimensions.thirtysecond_width = thirtysecond_dimensions.width;
    dimensions.thirtysecond_height = thirtysecond_dimensions.height;
    dimensions.postfx_usage = static_cast<RendererInterface::ResourceUsage>(
        RendererInterface::ResourceUsage::RENDER_TARGET |
        RendererInterface::ResourceUsage::COPY_SRC |
        RendererInterface::ResourceUsage::SHADER_RESOURCE |
        RendererInterface::ResourceUsage::UNORDER_ACCESS);
    return dimensions;
}

bool RendererSystemFrostedGlass::InitializeSharedPostFxResources(
    RendererInterface::ResourceOperator& resource_operator,
    const InitDimensions& dimensions)
{
    return m_postfx_shared_resources.Init(
        resource_operator,
        "PostFX_Shared",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
}

void RendererSystemFrostedGlass::CreateInitRenderTargets(
    RendererInterface::ResourceOperator& resource_operator,
    const InitDimensions& dimensions)
{
    auto& primary_blur_storage = m_primary_blur_pyramid;
    auto& multilayer_blur_storage = m_multilayer_blur_pyramid;
    auto& panel_payload_passes = m_panel_payload_passes;
    auto& composite_outputs = m_composite_outputs;
    auto& panel_payload_targets = m_panel_payload_targets;
    auto& primary_panel_payload = panel_payload_targets.primary;
    auto& secondary_panel_payload = panel_payload_targets.secondary;

    primary_blur_storage.Reset();
    multilayer_blur_storage.Reset();
    panel_payload_passes = {};
    m_legacy_composite_passes = {};
    m_shared_mip_composite_passes = {};
    composite_outputs = {};
    panel_payload_targets.Reset();

    primary_blur_storage.Half().ping = GetHalfResPing();
    primary_blur_storage.Half().pong = GetHalfResPong();
    primary_blur_storage.Quarter().ping = GetQuarterResPing();
    primary_blur_storage.Quarter().pong = GetQuarterResPong();

    composite_outputs.final_output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Output",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    composite_outputs.back_composite = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_BackComposite",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    primary_panel_payload.mask_parameter = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_MaskParameter",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    secondary_panel_payload.mask_parameter = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_MaskParameter_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    primary_panel_payload.panel_optics = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelOptics",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    secondary_panel_payload.panel_optics = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelOptics_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    primary_panel_payload.panel_profile = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelProfile",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    secondary_panel_payload.panel_profile = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelProfile_Secondary",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    primary_panel_payload.payload_depth = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelPayloadDepth",
        RendererInterface::D32,
        RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL));
    secondary_panel_payload.payload_depth = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_PanelPayloadDepth_Secondary",
        RendererInterface::D32,
        RendererInterface::default_clear_depth,
        static_cast<RendererInterface::ResourceUsage>(RendererInterface::ResourceUsage::DEPTH_STENCIL));

    multilayer_blur_storage.Half().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    multilayer_blur_storage.Half().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    multilayer_blur_storage.Quarter().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    multilayer_blur_storage.Quarter().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    primary_blur_storage.Eighth().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    primary_blur_storage.Eighth().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    primary_blur_storage.Sixteenth().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    primary_blur_storage.Sixteenth().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    primary_blur_storage.ThirtySecond().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    primary_blur_storage.ThirtySecond().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    multilayer_blur_storage.Eighth().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    multilayer_blur_storage.Eighth().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    multilayer_blur_storage.Sixteenth().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    multilayer_blur_storage.Sixteenth().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    multilayer_blur_storage.ThirtySecond().ping = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_Ping",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    multilayer_blur_storage.ThirtySecond().pong = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_Pong",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);

    primary_blur_storage.Half().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Half_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    primary_blur_storage.Quarter().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Quarter_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    primary_blur_storage.Eighth().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Eighth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    primary_blur_storage.Sixteenth().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Sixteenth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    primary_blur_storage.ThirtySecond().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_ThirtySecond_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);
    multilayer_blur_storage.Half().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Half_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.5f,
        0.5f,
        1,
        1);
    multilayer_blur_storage.Quarter().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Quarter_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.25f,
        0.25f,
        1,
        1);
    multilayer_blur_storage.Eighth().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Eighth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.125f,
        0.125f,
        1,
        1);
    multilayer_blur_storage.Sixteenth().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_Sixteenth_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.0625f,
        0.0625f,
        1,
        1);
    multilayer_blur_storage.ThirtySecond().output = resource_operator.CreateFrameBufferedWindowRelativeRenderTarget(
        "PostFX_Frosted_Multilayer_ThirtySecond_BlurFinal",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage,
        0.03125f,
        0.03125f,
        1,
        1);

    m_temporal_history_state.history_a = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_History_A",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
    m_temporal_history_state.history_b = resource_operator.CreateWindowRelativeRenderTarget(
        "PostFX_Frosted_History_B",
        RendererInterface::RGBA16_FLOAT,
        RendererInterface::default_clear_color,
        dimensions.postfx_usage);
}

void RendererSystemFrostedGlass::CreateInitBuffers(
    RendererInterface::ResourceOperator& resource_operator)
{
    RendererInterface::BufferDesc panel_data_buffer_desc{};
    panel_data_buffer_desc.name = "g_frosted_panels";
    panel_data_buffer_desc.size = sizeof(FrostedGlassPanelGpuData) * MAX_PANEL_COUNT;
    panel_data_buffer_desc.type = RendererInterface::DEFAULT;
    panel_data_buffer_desc.usage = RendererInterface::USAGE_SRV;
    m_frosted_panel_data_handle = resource_operator.CreateBuffer(panel_data_buffer_desc);

    RendererInterface::BufferDesc global_params_buffer_desc{};
    global_params_buffer_desc.name = "FrostedGlassGlobalBuffer";
    global_params_buffer_desc.size = sizeof(FrostedGlassGlobalParams);
    global_params_buffer_desc.type = RendererInterface::DEFAULT;
    global_params_buffer_desc.usage = RendererInterface::USAGE_CBV;
    m_frosted_global_params_handle = resource_operator.CreateBuffer(global_params_buffer_desc);
}

RendererSystemFrostedGlass::InitBindings RendererSystemFrostedGlass::BuildInitBindings() const
{
    GLTF_CHECK(m_frosted_panel_data_handle != NULL_HANDLE);
    GLTF_CHECK(m_frosted_global_params_handle != NULL_HANDLE);
    InitBindings bindings{};
    bindings.panel_data = RenderFeature::MakeStructuredBufferBinding(
        m_frosted_panel_data_handle,
        RendererInterface::BufferBindingDesc::SRV,
        sizeof(FrostedGlassPanelGpuData),
        MAX_PANEL_COUNT);
    bindings.global_params = RenderFeature::MakeConstantBufferBinding(m_frosted_global_params_handle);
    return bindings;
}

bool RendererSystemFrostedGlass::CreatePrimaryBlurPyramidPasses(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const InitDimensions& dimensions,
    const InitBindings& bindings,
    bool sync_existing)
{
    auto& primary_blur_storage = m_primary_blur_pyramid;
    auto& primary_half = primary_blur_storage.Half();
    auto& primary_quarter = primary_blur_storage.Quarter();
    auto& primary_eighth = primary_blur_storage.Eighth();
    auto& primary_sixteenth = primary_blur_storage.Sixteenth();
    auto& primary_thirtysecond = primary_blur_storage.ThirtySecond();

    const auto create_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            const char* entry_function,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height,
            bool bind_global_params) -> bool
    {
        auto builder = RenderFeature::MakeComputePostFxPassBuilder(
            "Frosted Glass",
            debug_name,
            entry_function,
            "Resources/Shaders/FrostedGlassPostfx.hlsl",
            input_rt,
            output_rt,
            dispatch_width,
            dispatch_height,
            dependency_node);
        if (bind_global_params)
        {
            builder.AddBuffer("FrostedGlassGlobalBuffer", bindings.global_params);
        }
        return RenderFeature::CreateOrSyncRenderGraphNode(
            sync_existing,
            resource_operator,
            graph,
            out_node_handle,
            builder.Build());
    };

    const auto create_downsample_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height) -> bool
    {
        return create_postfx_pass_node(
            out_node_handle,
            debug_name,
            "DownsampleMain",
            input_rt,
            output_rt,
            dependency_node,
            dispatch_width,
            dispatch_height,
            false);
    };

    const auto create_blur_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            const char* entry_function,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height) -> bool
    {
        return create_postfx_pass_node(
            out_node_handle,
            debug_name,
            entry_function,
            input_rt,
            output_rt,
            dependency_node,
            dispatch_width,
            dispatch_height,
            true);
    };

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_half.downsample_node,
        "Downsample Half",
        m_lighting->GetLightingOutput(),
        primary_half.ping,
        NULL_HANDLE,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_half.blur_horizontal_node,
        "Blur Half Horizontal",
        "BlurHorizontalMain",
        primary_half.ping,
        primary_half.pong,
        primary_half.downsample_node,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_half.blur_vertical_node,
        "Blur Half Vertical",
        "BlurVerticalMain",
        primary_half.pong,
        primary_half.output,
        primary_half.blur_horizontal_node,
        dimensions.half_width,
        dimensions.half_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_quarter.downsample_node,
        "Downsample Quarter",
        primary_half.output,
        primary_quarter.ping,
        primary_half.blur_vertical_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_quarter.blur_horizontal_node,
        "Blur Quarter Horizontal",
        "BlurHorizontalMain",
        primary_quarter.ping,
        primary_quarter.pong,
        primary_quarter.downsample_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_quarter.blur_vertical_node,
        "Blur Quarter Vertical",
        "BlurVerticalMain",
        primary_quarter.pong,
        primary_quarter.output,
        primary_quarter.blur_horizontal_node,
        dimensions.quarter_width,
        dimensions.quarter_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_eighth.downsample_node,
        "Downsample Eighth",
        primary_quarter.output,
        primary_eighth.ping,
        primary_quarter.blur_vertical_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_eighth.blur_horizontal_node,
        "Blur Eighth Horizontal",
        "BlurHorizontalMain",
        primary_eighth.ping,
        primary_eighth.pong,
        primary_eighth.downsample_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_eighth.blur_vertical_node,
        "Blur Eighth Vertical",
        "BlurVerticalMain",
        primary_eighth.pong,
        primary_eighth.output,
        primary_eighth.blur_horizontal_node,
        dimensions.eighth_width,
        dimensions.eighth_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_sixteenth.downsample_node,
        "Downsample Sixteenth",
        primary_eighth.output,
        primary_sixteenth.ping,
        primary_eighth.blur_vertical_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_sixteenth.blur_horizontal_node,
        "Blur Sixteenth Horizontal",
        "BlurHorizontalMain",
        primary_sixteenth.ping,
        primary_sixteenth.pong,
        primary_sixteenth.downsample_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_sixteenth.blur_vertical_node,
        "Blur Sixteenth Vertical",
        "BlurVerticalMain",
        primary_sixteenth.pong,
        primary_sixteenth.output,
        primary_sixteenth.blur_horizontal_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_thirtysecond.downsample_node,
        "Downsample ThirtySecond",
        primary_sixteenth.output,
        primary_thirtysecond.ping,
        primary_sixteenth.blur_vertical_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_thirtysecond.blur_horizontal_node,
        "Blur ThirtySecond Horizontal",
        "BlurHorizontalMain",
        primary_thirtysecond.ping,
        primary_thirtysecond.pong,
        primary_thirtysecond.downsample_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        primary_thirtysecond.blur_vertical_node,
        "Blur ThirtySecond Vertical",
        "BlurVerticalMain",
        primary_thirtysecond.pong,
        primary_thirtysecond.output,
        primary_thirtysecond.blur_horizontal_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_half.shared_downsample_node,
        "SharedMip Downsample Half",
        m_lighting->GetLightingOutput(),
        primary_half.output,
        NULL_HANDLE,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_quarter.shared_downsample_node,
        "SharedMip Downsample Quarter",
        primary_half.output,
        primary_quarter.output,
        primary_half.shared_downsample_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_eighth.shared_downsample_node,
        "SharedMip Downsample Eighth",
        primary_quarter.output,
        primary_eighth.output,
        primary_quarter.shared_downsample_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_sixteenth.shared_downsample_node,
        "SharedMip Downsample Sixteenth",
        primary_eighth.output,
        primary_sixteenth.output,
        primary_eighth.shared_downsample_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        primary_thirtysecond.shared_downsample_node,
        "SharedMip Downsample ThirtySecond",
        primary_sixteenth.output,
        primary_thirtysecond.output,
        primary_sixteenth.shared_downsample_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));
    return true;
}

bool RendererSystemFrostedGlass::CreatePanelPayloadPasses(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
    const InitDimensions& dimensions,
    const InitBindings& bindings,
    bool sync_existing)
{
    auto& panel_payload_passes = m_panel_payload_passes;
    const auto& primary_panel_payload = m_panel_payload_targets.primary;
    const auto& secondary_panel_payload = m_panel_payload_targets.secondary;

    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        sync_existing,
        resource_operator,
        graph,
        panel_payload_passes.compute,
        RenderFeature::PassBuilder::Compute("Frosted Glass", "Frosted Mask/Parameter")
            .AddModule(m_scene->GetCameraModule())
            .AddShader(RendererInterface::COMPUTE_SHADER, "MaskParameterMain", "Resources/Shaders/FrostedGlass.hlsl")
            .AddSampledRenderTargetBindings({
                RenderFeature::MakeSampledRenderTargetBinding(
                    "InputDepthTex",
                    scene_outputs.depth,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "InputNormalTex",
                    scene_outputs.normal,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamOutput",
                    primary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamSecondaryOutput",
                    secondary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsOutput",
                    primary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsSecondaryOutput",
                    secondary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileOutput",
                    primary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileSecondaryOutput",
                    secondary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV)
            })
            .AddBuffers({
                RenderFeature::MakeBufferBinding("g_frosted_panels", bindings.panel_data),
                RenderFeature::MakeBufferBinding("FrostedGlassGlobalBuffer", bindings.global_params)
            })
            .SetDispatch2D(dimensions.width, dimensions.height)
            .Build()));

    RendererInterface::RenderExecuteCommand panel_payload_draw_command{};
    panel_payload_draw_command.type = RendererInterface::ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.vertex_count = 6;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.instance_count = MAX_PANEL_COUNT;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.start_vertex_location = 0;
    panel_payload_draw_command.parameter.draw_vertex_instance_command_parameter.start_instance_location = 0;
    const auto panel_payload_graphics_plan =
        RenderFeature::GraphicsExecutionPlan::FromExtent(dimensions.width, dimensions.height);

    const auto create_panel_payload_raster_setup_info =
        [&](const char* debug_name,
            const char* pixel_shader_entry,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            RendererInterface::RenderTargetHandle mask_output,
            RendererInterface::RenderTargetHandle optics_output,
            RendererInterface::RenderTargetHandle profile_output,
            RendererInterface::RenderTargetHandle depth_output,
            bool include_front_mask)
        -> RendererInterface::RenderGraph::RenderPassSetupInfo
    {
        RendererInterface::RenderStateDesc render_state{};
        render_state.depth_stencil_mode = RendererInterface::DepthStencilMode::DEPTH_WRITE;

        auto builder = RenderFeature::PassBuilder::Graphics("Frosted Glass", debug_name);
        builder
            .SetRenderState(render_state)
            .SetViewport(panel_payload_graphics_plan)
            .AddModule(m_scene->GetCameraModule())
            .AddShader(RendererInterface::VERTEX_SHADER, "PanelPayloadVS", "Resources/Shaders/FrostedGlass.hlsl")
            .AddShader(RendererInterface::FRAGMENT_SHADER, pixel_shader_entry, "Resources/Shaders/FrostedGlass.hlsl")
            .AddDependency(dependency_node)
            .AddRenderTargets({
                RenderFeature::MakeRenderTargetAttachment(
                    mask_output,
                    RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA16_FLOAT, true)),
                RenderFeature::MakeRenderTargetAttachment(
                    optics_output,
                    RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA16_FLOAT, true)),
                RenderFeature::MakeRenderTargetAttachment(
                    profile_output,
                    RenderFeature::MakeColorRenderTargetBinding(RendererInterface::RGBA16_FLOAT, true)),
                RenderFeature::MakeRenderTargetAttachment(
                    depth_output,
                    RenderFeature::MakeDepthRenderTargetBinding(RendererInterface::D32, true))
            })
            .AddSampledRenderTargetBindings({
                RenderFeature::MakeSampledRenderTargetBinding(
                    "InputDepthTex",
                    scene_outputs.depth,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "InputNormalTex",
                    scene_outputs.normal,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV)
            })
            .AddBuffers({
                RenderFeature::MakeBufferBinding("g_frosted_panels", bindings.panel_data),
                RenderFeature::MakeBufferBinding("FrostedGlassGlobalBuffer", bindings.global_params)
            })
            .SetExecuteCommand(panel_payload_draw_command);

        if (include_front_mask)
        {
            builder.AddSampledRenderTarget(
                "FrontMaskParamTex",
                primary_panel_payload.mask_parameter,
                RendererInterface::RenderTargetTextureBindingDesc::SRV);
        }

        return builder.Build();
    };

    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        sync_existing,
        resource_operator,
        graph,
        panel_payload_passes.raster_front,
        create_panel_payload_raster_setup_info(
            "Frosted Mask/Parameter Raster Front",
            "PanelPayloadFrontPS",
            NULL_HANDLE,
            primary_panel_payload.mask_parameter,
            primary_panel_payload.panel_optics,
            primary_panel_payload.panel_profile,
            primary_panel_payload.payload_depth,
            false)));

    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        sync_existing,
        resource_operator,
        graph,
        panel_payload_passes.raster_back,
        create_panel_payload_raster_setup_info(
            "Frosted Mask/Parameter Raster Back",
            "PanelPayloadBackPS",
            panel_payload_passes.raster_front,
            secondary_panel_payload.mask_parameter,
            secondary_panel_payload.panel_optics,
            secondary_panel_payload.panel_profile,
            secondary_panel_payload.payload_depth,
            true)));
    return true;
}

bool RendererSystemFrostedGlass::CreateCompositePasses(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
    const InitDimensions& dimensions,
    const InitBindings& bindings,
    bool sync_existing)
{
    const auto primary_blur = GetPrimaryBlurPyramidView();
    const auto multilayer_blur = GetMultilayerBlurPyramidView();
    auto& legacy_composite = m_legacy_composite_passes;
    auto& shared_mip_composite = m_shared_mip_composite_passes;
    auto& multilayer_blur_storage = m_multilayer_blur_pyramid;
    const auto& composite_outputs = m_composite_outputs;
    const auto& primary_panel_payload = m_panel_payload_targets.primary;
    const auto& secondary_panel_payload = m_panel_payload_targets.secondary;
    const RendererInterface::RenderTargetHandle velocity_rt = scene_outputs.velocity;
    auto& multilayer_half = multilayer_blur_storage.Half();
    auto& multilayer_quarter = multilayer_blur_storage.Quarter();
    auto& multilayer_eighth = multilayer_blur_storage.Eighth();
    auto& multilayer_sixteenth = multilayer_blur_storage.Sixteenth();
    auto& multilayer_thirtysecond = multilayer_blur_storage.ThirtySecond();

    const auto create_frosted_composite_back_setup_info =
        [&](const char* debug_name,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            const auto& blur_pyramid)
        -> RendererInterface::RenderGraph::RenderPassSetupInfo
    {
        auto builder = RenderFeature::PassBuilder::Compute("Frosted Glass", debug_name);
        builder
            .AddModule(m_scene->GetCameraModule())
            .AddShader(RendererInterface::COMPUTE_SHADER, "CompositeBackMain", "Resources/Shaders/FrostedGlass.hlsl")
            .AddDependency(dependency_node)
            .AddSampledRenderTargetBindings({
                RenderFeature::MakeSampledRenderTargetBinding(
                    "InputColorTex",
                    m_lighting->GetLightingOutput(),
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "BlurredColorTex",
                    blur_pyramid.Half().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "QuarterBlurredColorTex",
                    blur_pyramid.Quarter().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "EighthBlurredColorTex",
                    blur_pyramid.Eighth().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "SixteenthBlurredColorTex",
                    blur_pyramid.Sixteenth().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "ThirtySecondBlurredColorTex",
                    blur_pyramid.ThirtySecond().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamTex",
                    primary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamSecondaryTex",
                    secondary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsTex",
                    primary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsSecondaryTex",
                    secondary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileTex",
                    primary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileSecondaryTex",
                    secondary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "BackCompositeOutput",
                    composite_outputs.back_composite,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV)
            })
            .AddBuffers({
                RenderFeature::MakeBufferBinding("g_frosted_panels", bindings.panel_data),
                RenderFeature::MakeBufferBinding("FrostedGlassGlobalBuffer", bindings.global_params)
            })
            .SetDispatch2D(dimensions.width, dimensions.height);
        return builder.Build();
    };

    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        sync_existing,
        resource_operator,
        graph,
        legacy_composite.back,
        create_frosted_composite_back_setup_info(
            "Frosted Composite Back",
            primary_blur.ThirtySecond().blur_vertical_node,
            primary_blur)));
    RETURN_IF_FALSE(RenderFeature::CreateOrSyncRenderGraphNode(
        sync_existing,
        resource_operator,
        graph,
        shared_mip_composite.back,
        create_frosted_composite_back_setup_info(
            "Frosted Composite Back SharedMip",
            primary_blur.ThirtySecond().shared_downsample_node,
            primary_blur)));

    const auto create_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            const char* entry_function,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height,
            bool bind_global_params) -> bool
    {
        auto builder = RenderFeature::MakeComputePostFxPassBuilder(
            "Frosted Glass",
            debug_name,
            entry_function,
            "Resources/Shaders/FrostedGlassPostfx.hlsl",
            input_rt,
            output_rt,
            dispatch_width,
            dispatch_height,
            dependency_node);
        if (bind_global_params)
        {
            builder.AddBuffer("FrostedGlassGlobalBuffer", bindings.global_params);
        }
        return RenderFeature::CreateOrSyncRenderGraphNode(
            sync_existing,
            resource_operator,
            graph,
            out_node_handle,
            builder.Build());
    };

    const auto create_downsample_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height) -> bool
    {
        return create_postfx_pass_node(
            out_node_handle,
            debug_name,
            "DownsampleMain",
            input_rt,
            output_rt,
            dependency_node,
            dispatch_width,
            dispatch_height,
            false);
    };

    const auto create_blur_postfx_pass_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            const char* entry_function,
            RendererInterface::RenderTargetHandle input_rt,
            RendererInterface::RenderTargetHandle output_rt,
            RendererInterface::RenderGraphNodeHandle dependency_node,
            unsigned dispatch_width,
            unsigned dispatch_height) -> bool
    {
        return create_postfx_pass_node(
            out_node_handle,
            debug_name,
            entry_function,
            input_rt,
            output_rt,
            dependency_node,
            dispatch_width,
            dispatch_height,
            true);
    };

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_half.downsample_node,
        "Downsample Half Multilayer",
        composite_outputs.back_composite,
        multilayer_half.ping,
        legacy_composite.back,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_half.blur_horizontal_node,
        "Blur Half Multilayer Horizontal",
        "BlurHorizontalMain",
        multilayer_half.ping,
        multilayer_half.pong,
        multilayer_half.downsample_node,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_half.blur_vertical_node,
        "Blur Half Multilayer Vertical",
        "BlurVerticalMain",
        multilayer_half.pong,
        multilayer_half.output,
        multilayer_half.blur_horizontal_node,
        dimensions.half_width,
        dimensions.half_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_quarter.downsample_node,
        "Downsample Quarter Multilayer",
        multilayer_half.output,
        multilayer_quarter.ping,
        multilayer_half.blur_vertical_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_quarter.blur_horizontal_node,
        "Blur Quarter Multilayer Horizontal",
        "BlurHorizontalMain",
        multilayer_quarter.ping,
        multilayer_quarter.pong,
        multilayer_quarter.downsample_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_quarter.blur_vertical_node,
        "Blur Quarter Multilayer Vertical",
        "BlurVerticalMain",
        multilayer_quarter.pong,
        multilayer_quarter.output,
        multilayer_quarter.blur_horizontal_node,
        dimensions.quarter_width,
        dimensions.quarter_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_eighth.downsample_node,
        "Downsample Eighth Multilayer",
        multilayer_quarter.output,
        multilayer_eighth.ping,
        multilayer_quarter.blur_vertical_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_eighth.blur_horizontal_node,
        "Blur Eighth Multilayer Horizontal",
        "BlurHorizontalMain",
        multilayer_eighth.ping,
        multilayer_eighth.pong,
        multilayer_eighth.downsample_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_eighth.blur_vertical_node,
        "Blur Eighth Multilayer Vertical",
        "BlurVerticalMain",
        multilayer_eighth.pong,
        multilayer_eighth.output,
        multilayer_eighth.blur_horizontal_node,
        dimensions.eighth_width,
        dimensions.eighth_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_sixteenth.downsample_node,
        "Downsample Sixteenth Multilayer",
        multilayer_eighth.output,
        multilayer_sixteenth.ping,
        multilayer_eighth.blur_vertical_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_sixteenth.blur_horizontal_node,
        "Blur Sixteenth Multilayer Horizontal",
        "BlurHorizontalMain",
        multilayer_sixteenth.ping,
        multilayer_sixteenth.pong,
        multilayer_sixteenth.downsample_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_sixteenth.blur_vertical_node,
        "Blur Sixteenth Multilayer Vertical",
        "BlurVerticalMain",
        multilayer_sixteenth.pong,
        multilayer_sixteenth.output,
        multilayer_sixteenth.blur_horizontal_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_thirtysecond.downsample_node,
        "Downsample ThirtySecond Multilayer",
        multilayer_sixteenth.output,
        multilayer_thirtysecond.ping,
        multilayer_sixteenth.blur_vertical_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_thirtysecond.blur_horizontal_node,
        "Blur ThirtySecond Multilayer Horizontal",
        "BlurHorizontalMain",
        multilayer_thirtysecond.ping,
        multilayer_thirtysecond.pong,
        multilayer_thirtysecond.downsample_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));
    RETURN_IF_FALSE(create_blur_postfx_pass_node(
        multilayer_thirtysecond.blur_vertical_node,
        "Blur ThirtySecond Multilayer Vertical",
        "BlurVerticalMain",
        multilayer_thirtysecond.pong,
        multilayer_thirtysecond.output,
        multilayer_thirtysecond.blur_horizontal_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));

    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_half.shared_downsample_node,
        "SharedMip Downsample Half Multilayer",
        composite_outputs.back_composite,
        multilayer_half.output,
        shared_mip_composite.back,
        dimensions.half_width,
        dimensions.half_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_quarter.shared_downsample_node,
        "SharedMip Downsample Quarter Multilayer",
        multilayer_half.output,
        multilayer_quarter.output,
        multilayer_half.shared_downsample_node,
        dimensions.quarter_width,
        dimensions.quarter_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_eighth.shared_downsample_node,
        "SharedMip Downsample Eighth Multilayer",
        multilayer_quarter.output,
        multilayer_eighth.output,
        multilayer_quarter.shared_downsample_node,
        dimensions.eighth_width,
        dimensions.eighth_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_sixteenth.shared_downsample_node,
        "SharedMip Downsample Sixteenth Multilayer",
        multilayer_eighth.output,
        multilayer_sixteenth.output,
        multilayer_eighth.shared_downsample_node,
        dimensions.sixteenth_width,
        dimensions.sixteenth_height));
    RETURN_IF_FALSE(create_downsample_postfx_pass_node(
        multilayer_thirtysecond.shared_downsample_node,
        "SharedMip Downsample ThirtySecond Multilayer",
        multilayer_sixteenth.output,
        multilayer_thirtysecond.output,
        multilayer_sixteenth.shared_downsample_node,
        dimensions.thirtysecond_width,
        dimensions.thirtysecond_height));

    const auto create_temporal_frosted_composite_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            const char* entry_function,
            const char* primary_input_binding_name,
            RendererInterface::RenderTargetHandle primary_input_rt,
            const auto& blur_pyramid,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write,
            RendererInterface::RenderGraphNodeHandle dependency_node)
        -> bool
    {
        auto builder = RenderFeature::PassBuilder::Compute("Frosted Glass", debug_name);
        builder
            .AddModule(m_scene->GetCameraModule())
            .AddShader(RendererInterface::COMPUTE_SHADER, entry_function, "Resources/Shaders/FrostedGlass.hlsl")
            .AddDependency(dependency_node)
            .AddSampledRenderTargetBindings({
                RenderFeature::MakeSampledRenderTargetBinding(
                    primary_input_binding_name,
                    primary_input_rt,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "BlurredColorTex",
                    blur_pyramid.Half().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "QuarterBlurredColorTex",
                    blur_pyramid.Quarter().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "EighthBlurredColorTex",
                    blur_pyramid.Eighth().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "SixteenthBlurredColorTex",
                    blur_pyramid.Sixteenth().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "ThirtySecondBlurredColorTex",
                    blur_pyramid.ThirtySecond().output,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamTex",
                    primary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "MaskParamSecondaryTex",
                    secondary_panel_payload.mask_parameter,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsTex",
                    primary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelOpticsSecondaryTex",
                    secondary_panel_payload.panel_optics,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileTex",
                    primary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "PanelProfileSecondaryTex",
                    secondary_panel_payload.panel_profile,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "HistoryInputTex",
                    history_read,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "VelocityTex",
                    velocity_rt,
                    RendererInterface::RenderTargetTextureBindingDesc::SRV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "Output",
                    composite_outputs.final_output,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV),
                RenderFeature::MakeSampledRenderTargetBinding(
                    "HistoryOutputTex",
                    history_write,
                    RendererInterface::RenderTargetTextureBindingDesc::UAV)
            })
            .AddBuffers({
                RenderFeature::MakeBufferBinding("g_frosted_panels", bindings.panel_data),
                RenderFeature::MakeBufferBinding("FrostedGlassGlobalBuffer", bindings.global_params)
            })
            .SetDispatch2D(dimensions.width, dimensions.height);
        return RenderFeature::CreateOrSyncRenderGraphNode(
            sync_existing,
            resource_operator,
            graph,
            out_node_handle,
            builder.Build());
    };

    const auto create_frosted_front_composite_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write,
            RendererInterface::RenderGraphNodeHandle dependency_node)
        -> bool
    {
        return create_temporal_frosted_composite_node(
            out_node_handle,
            debug_name,
            "CompositeFrontMain",
            "BackCompositeTex",
            composite_outputs.back_composite,
            multilayer_blur,
            history_read,
            history_write,
            dependency_node);
    };

    const auto create_temporal_history_node_pair =
        [&](TemporalHistoryNodePairView& out_history_pair,
            const char* debug_name_ab,
            const char* debug_name_ba,
            const auto& create_node)
        -> bool
    {
        RETURN_IF_FALSE(create_node(
            out_history_pair.history_ab,
            debug_name_ab,
            m_temporal_history_state.history_a,
            m_temporal_history_state.history_b));
        RETURN_IF_FALSE(create_node(
            out_history_pair.history_ba,
            debug_name_ba,
            m_temporal_history_state.history_b,
            m_temporal_history_state.history_a));
        return true;
    };

    RETURN_IF_FALSE(create_temporal_history_node_pair(
        legacy_composite.front_history,
        "Frosted Composite Front History A->B",
        "Frosted Composite Front History B->A",
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write) -> bool
        {
            return create_frosted_front_composite_node(
                out_node_handle,
                debug_name,
                history_read,
                history_write,
                multilayer_blur.ThirtySecond().blur_vertical_node);
        }));

    RETURN_IF_FALSE(create_temporal_history_node_pair(
        shared_mip_composite.front_history,
        "Frosted Composite Front SharedMip History A->B",
        "Frosted Composite Front SharedMip History B->A",
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write) -> bool
        {
            return create_frosted_front_composite_node(
                out_node_handle,
                debug_name,
                history_read,
                history_write,
                multilayer_blur.ThirtySecond().shared_downsample_node);
        }));

    const auto create_frosted_composite_node =
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write,
            RendererInterface::RenderGraphNodeHandle dependency_node)
        -> bool
    {
        return create_temporal_frosted_composite_node(
            out_node_handle,
            debug_name,
            "CompositeMain",
            "InputColorTex",
            m_lighting->GetLightingOutput(),
            primary_blur,
            history_read,
            history_write,
            dependency_node);
    };

    RETURN_IF_FALSE(create_temporal_history_node_pair(
        legacy_composite.single_history,
        "Frosted Composite History A->B",
        "Frosted Composite History B->A",
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write) -> bool
        {
            return create_frosted_composite_node(
                out_node_handle,
                debug_name,
                history_read,
                history_write,
                primary_blur.ThirtySecond().blur_vertical_node);
        }));

    RETURN_IF_FALSE(create_temporal_history_node_pair(
        shared_mip_composite.single_history,
        "Frosted Composite SharedMip History A->B",
        "Frosted Composite SharedMip History B->A",
        [&](RendererInterface::RenderGraphNodeHandle& out_node_handle,
            const char* debug_name,
            RendererInterface::RenderTargetHandle history_read,
            RendererInterface::RenderTargetHandle history_write) -> bool
        {
            return create_frosted_composite_node(
                out_node_handle,
                debug_name,
                history_read,
                history_write,
                primary_blur.ThirtySecond().shared_downsample_node);
        }));
    return true;
}

bool RendererSystemFrostedGlass::SyncStaticPassSetups(
    RendererInterface::ResourceOperator& resource_operator,
    RendererInterface::RenderGraph& graph,
    const RendererSystemSceneRenderer::BasePassOutputs& scene_outputs,
    const InitDimensions& dimensions,
    const InitBindings& bindings)
{
    const RenderFeature::FrameDimensions frame_dimensions =
        RenderFeature::FrameDimensions::FromExtent(dimensions.width, dimensions.height);
    if (m_static_pass_setup_state.Matches(frame_dimensions))
    {
        return true;
    }

    RETURN_IF_FALSE(CreatePrimaryBlurPyramidPasses(resource_operator, graph, dimensions, bindings, true));
    RETURN_IF_FALSE(CreatePanelPayloadPasses(resource_operator, graph, scene_outputs, dimensions, bindings, true));
    RETURN_IF_FALSE(CreateCompositePasses(resource_operator, graph, scene_outputs, dimensions, bindings, true));

    m_static_pass_setup_state.Update(frame_dimensions);
    return true;
}

bool RendererSystemFrostedGlass::Init(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph)
{
    GLTF_CHECK(m_scene);
    GLTF_CHECK(m_lighting);
    GLTF_CHECK(m_scene->HasInit());
    GLTF_CHECK(m_lighting->HasInit());

    ResetInitRuntimeState();
    const InitDimensions dimensions = BuildInitDimensions(resource_operator);
    GLTF_CHECK(InitializeSharedPostFxResources(resource_operator, dimensions));
    CreateInitRenderTargets(resource_operator, dimensions);
    CreateInitBuffers(resource_operator);
    const InitBindings bindings = BuildInitBindings();
    const auto scene_outputs = m_scene->GetOutputs();

    RETURN_IF_FALSE(CreatePrimaryBlurPyramidPasses(resource_operator, graph, dimensions, bindings));
    RETURN_IF_FALSE(CreatePanelPayloadPasses(resource_operator, graph, scene_outputs, dimensions, bindings));
    RETURN_IF_FALSE(CreateCompositePasses(resource_operator, graph, scene_outputs, dimensions, bindings));

    m_static_pass_setup_state.Update(RenderFeature::FrameDimensions::FromExtent(dimensions.width, dimensions.height));
    UploadPanelData(resource_operator);
    graph.RegisterRenderTargetToColorOutput(m_composite_outputs.final_output);
    return true;
}

bool RendererSystemFrostedGlass::HasInit() const
{
    const auto primary_blur = GetPrimaryBlurPyramidView();
    const auto multilayer_blur = GetMultilayerBlurPyramidView();
    const auto legacy_composite = GetLegacyCompositePassBundleView();
    const auto shared_mip_composite = GetSharedMipCompositePassBundleView();

    return primary_blur.HasLegacyPath() &&
           primary_blur.HasSharedMipPath() &&
           m_panel_payload_passes.HasComputePath() &&
           legacy_composite.HasInit() &&
           multilayer_blur.HasLegacyPath() &&
           multilayer_blur.HasSharedMipPath() &&
           shared_mip_composite.HasInit() &&
           m_composite_outputs.HasInit() &&
           m_panel_payload_targets.HasInit() &&
           m_temporal_history_state.HasTargets() &&
           m_postfx_shared_resources.HasInit();
}

void RendererSystemFrostedGlass::ResetRuntimeResources(RendererInterface::ResourceOperator& resource_operator)
{
    (void)resource_operator;
    m_postfx_shared_resources.Reset();
    m_primary_blur_pyramid.Reset();
    m_multilayer_blur_pyramid.Reset();
    m_panel_payload_passes = {};
    m_legacy_composite_passes = {};
    m_shared_mip_composite_passes = {};
    m_composite_outputs = {};
    m_panel_payload_targets.Reset();
    m_temporal_history_state.ResetRuntime();
    m_global_params.temporal_history_valid = 0;
    m_static_pass_setup_state.Reset();
    m_frosted_panel_data_handle = NULL_HANDLE;
    m_frosted_global_params_handle = NULL_HANDLE;
    m_need_upload_panels = true;
    m_need_upload_global_params = true;
    m_dispatch_cache_state.Reset();
    m_panel_payload_compute_fallback_active = false;
    m_last_expected_registered_pass_count = 0;
}

void RendererSystemFrostedGlass::UpdateDirectionalHighlightParams()
{
    glm::fvec4 next_highlight_light = {0.0f, -1.0f, 0.0f, 0.0f};
    if (m_lighting)
    {
        glm::fvec3 dominant_light_dir{0.0f, -1.0f, 0.0f};
        float dominant_light_luminance = 0.0f;
        if (m_lighting->GetDominantDirectionalLight(dominant_light_dir, dominant_light_luminance))
        {
            (void)dominant_light_luminance;
            next_highlight_light = {
                dominant_light_dir.x,
                dominant_light_dir.y,
                dominant_light_dir.z,
                1.0f
            };
        }
    }

    const auto nearly_equal = [](float lhs, float rhs) -> bool
    {
        return std::fabs(lhs - rhs) <= 1e-4f;
    };

    const auto& current = m_global_params.highlight_light_dir_weight;
    if (!nearly_equal(current.x, next_highlight_light.x) ||
        !nearly_equal(current.y, next_highlight_light.y) ||
        !nearly_equal(current.z, next_highlight_light.z) ||
        !nearly_equal(current.w, next_highlight_light.w))
    {
        m_global_params.highlight_light_dir_weight = next_highlight_light;
        m_need_upload_global_params = true;
    }
}

RendererSystemFrostedGlass::BlurRuntimeResolution RendererSystemFrostedGlass::ResolveBlurRuntimeMode()
{
    BlurRuntimeResolution resolution{};
    resolution.effective_mode = m_blur_source_mode;
    if (resolution.effective_mode == BlurSourceMode::SharedDual)
    {
        // SharedDual is reserved as an optional quality tier in B8; current runtime falls back to SharedMip.
        resolution.effective_mode = BlurSourceMode::SharedMip;
    }
    resolution.use_shared_mip_path = resolution.effective_mode != BlurSourceMode::LegacyPyramid;

    const unsigned blur_source_mode_flag = static_cast<unsigned>(resolution.effective_mode);
    if (m_global_params.blur_source_mode != blur_source_mode_flag)
    {
        m_global_params.blur_source_mode = blur_source_mode_flag;
        m_need_upload_global_params = true;
    }

    return resolution;
}

RendererSystemFrostedGlass::MultilayerRuntimeResolution RendererSystemFrostedGlass::ResolveMultilayerRuntimeState(
    unsigned long long interval)
{
    MultilayerRuntimeResolution resolution{};
    if (m_global_params.multilayer_mode == MULTILAYER_MODE_SINGLE)
    {
        m_multilayer_over_budget_streak = 0;
        m_multilayer_cooldown_frames = 0;
        resolution.runtime_enabled = false;
    }
    else if (m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE)
    {
        m_multilayer_over_budget_streak = 0;
        m_multilayer_cooldown_frames = 0;
        resolution.runtime_enabled = true;
    }
    else
    {
        const float frame_ms = static_cast<float>(interval);
        const float frame_budget_ms = (std::max)(m_global_params.multilayer_frame_budget_ms, 1.0f);
        if (m_multilayer_cooldown_frames > 0)
        {
            --m_multilayer_cooldown_frames;
            m_multilayer_over_budget_streak = 0;
            resolution.runtime_enabled = false;
        }
        else
        {
            if (frame_ms > frame_budget_ms)
            {
                ++m_multilayer_over_budget_streak;
            }
            else if (m_multilayer_over_budget_streak > 0)
            {
                --m_multilayer_over_budget_streak;
            }

            if (m_multilayer_over_budget_streak >= MULTILAYER_AUTO_OVER_BUDGET_TRIGGER_FRAMES)
            {
                m_multilayer_over_budget_streak = 0;
                m_multilayer_cooldown_frames = MULTILAYER_AUTO_COOLDOWN_FRAMES;
                resolution.runtime_enabled = false;
            }
            else
            {
                resolution.runtime_enabled = true;
            }
        }
    }

    if (m_multilayer_runtime_enabled != resolution.runtime_enabled)
    {
        m_multilayer_runtime_enabled = resolution.runtime_enabled;
        m_need_upload_global_params = true;
    }
    const unsigned multilayer_runtime_flag = m_multilayer_runtime_enabled ? 1u : 0u;
    if (m_global_params.multilayer_runtime_enabled != multilayer_runtime_flag)
    {
        m_global_params.multilayer_runtime_enabled = multilayer_runtime_flag;
        m_need_upload_global_params = true;
    }
    resolution.use_strict_multilayer_path =
        m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE &&
        m_multilayer_runtime_enabled;

    return resolution;
}

void RendererSystemFrostedGlass::SyncTickTemporalHistoryFlags()
{
    // Keep frosted history across regular camera motion; per-pixel velocity/edge rejection handles reprojection risk.
    // Reserve hard reset for explicit local events (resize/UI reset/mode switch).
    if (m_temporal_history_state.force_reset)
    {
        m_temporal_history_state.valid = false;
    }
    m_temporal_history_state.force_reset = false;

    const unsigned history_valid_flag = m_temporal_history_state.valid ? 1u : 0u;
    if (m_global_params.temporal_history_valid != history_valid_flag)
    {
        m_global_params.temporal_history_valid = history_valid_flag;
        m_need_upload_global_params = true;
    }
}

void RendererSystemFrostedGlass::ResolveTickCompositePasses(TickRuntimePaths& runtime_paths) const
{
    runtime_paths.primary_blur = GetPrimaryBlurPyramidView();
    runtime_paths.multilayer_blur = GetMultilayerBlurPyramidView();

    const CompositePassBundleView legacy_composite = GetLegacyCompositePassBundleView();
    const CompositePassBundleView shared_mip_composite = GetSharedMipCompositePassBundleView();
    runtime_paths.active_composite = runtime_paths.use_shared_mip_path ? shared_mip_composite : legacy_composite;
    runtime_paths.active_single_composite_pass =
        runtime_paths.active_composite.SelectSingle(m_temporal_history_state.read_is_a);
    runtime_paths.active_front_composite_pass =
        runtime_paths.active_composite.SelectFront(m_temporal_history_state.read_is_a);
    runtime_paths.active_back_composite_pass = runtime_paths.active_composite.back;
}

void RendererSystemFrostedGlass::ResolveTickPanelPayloadPath(TickRuntimePaths& runtime_paths)
{
    const bool request_raster_panel_payload = m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer;
    runtime_paths.use_raster_panel_payload = request_raster_panel_payload && m_panel_payload_passes.HasRasterPath();
    m_panel_payload_compute_fallback_active =
        request_raster_panel_payload && !runtime_paths.use_raster_panel_payload;
}

RendererSystemFrostedGlass::TickRuntimePaths RendererSystemFrostedGlass::BuildTickRuntimePaths(
    unsigned long long interval)
{
    TickRuntimePaths runtime_paths{};

    SyncTickTemporalHistoryFlags();

    const BlurRuntimeResolution blur_runtime = ResolveBlurRuntimeMode();
    const MultilayerRuntimeResolution multilayer_runtime = ResolveMultilayerRuntimeState(interval);

    runtime_paths.use_shared_mip_path = blur_runtime.use_shared_mip_path;
    runtime_paths.use_strict_multilayer_path = multilayer_runtime.use_strict_multilayer_path;

    ResolveTickCompositePasses(runtime_paths);
    ResolveTickPanelPayloadPath(runtime_paths);

    UpdateTickRuntimeSummary(runtime_paths);
    return runtime_paths;
}

RendererSystemFrostedGlass::TickExecutionPlan RendererSystemFrostedGlass::BuildTickExecutionPlan(
    unsigned long long interval,
    unsigned width,
    unsigned height)
{
    TickExecutionPlan execution_plan{};
    execution_plan.render_width = width;
    execution_plan.render_height = height;
    execution_plan.runtime_paths = BuildTickRuntimePaths(interval);
    execution_plan.dispatch_dimensions = BuildTickDispatchDimensions(width, height);
    return execution_plan;
}

void RendererSystemFrostedGlass::UpdateTickRuntimeSummary(const TickRuntimePaths& runtime_paths)
{
    const unsigned blur_level_count = static_cast<unsigned>(runtime_paths.primary_blur.levels.size());
    unsigned expected_pass_count = 0u;
    expected_pass_count += runtime_paths.use_shared_mip_path ? blur_level_count : blur_level_count * 3u;
    expected_pass_count += runtime_paths.use_raster_panel_payload ? 2u : 1u;
    if (runtime_paths.use_strict_multilayer_path)
    {
        expected_pass_count +=
            runtime_paths.use_shared_mip_path ? blur_level_count + 2u : blur_level_count * 3u + 2u;
    }
    else
    {
        expected_pass_count += 1u;
    }

    m_last_expected_registered_pass_count = expected_pass_count;
    m_last_runtime_used_shared_mip_path = runtime_paths.use_shared_mip_path;
    m_last_runtime_used_raster_payload = runtime_paths.use_raster_panel_payload;
    m_last_runtime_used_strict_multilayer = runtime_paths.use_strict_multilayer_path;
}

void RendererSystemFrostedGlass::RefreshTickPanelData(
    RendererInterface::ResourceOperator& resource_operator,
    float delta_seconds)
{
    UpdateDirectionalHighlightParams();
    RefreshExternalPanelsFromProducers();
    ApplyExternalPanelDebugOverrides();
    UpdatePanelRuntimeStates(delta_seconds);
    UploadPanelData(resource_operator);
}

void RendererSystemFrostedGlass::UpdateTickBlurDispatch(RendererInterface::RenderGraph& graph,
                                                        const BlurPyramidBundleView& blur_pyramid,
                                                        const TickDispatchDimensions& dispatch_dimensions,
                                                        bool use_shared_mip_path)
{
    for (size_t level_index = 0; level_index < blur_pyramid.levels.size(); ++level_index)
    {
        const BlurPyramidLevelView& level = blur_pyramid.levels[level_index];
        const std::pair<unsigned, unsigned>& dispatch_size = dispatch_dimensions.blur_dispatch_sizes[level_index];
        if (use_shared_mip_path)
        {
            graph.UpdateComputeDispatch(level.shared_downsample_node, dispatch_size.first, dispatch_size.second, 1);
        }
        else
        {
            graph.UpdateComputeDispatch(level.downsample_node, dispatch_size.first, dispatch_size.second, 1);
            graph.UpdateComputeDispatch(level.blur_horizontal_node, dispatch_size.first, dispatch_size.second, 1);
            graph.UpdateComputeDispatch(level.blur_vertical_node, dispatch_size.first, dispatch_size.second, 1);
        }
    }
}

void RendererSystemFrostedGlass::UpdateTickPanelPayloadDispatch(RendererInterface::RenderGraph& graph,
                                                                const TickRuntimePaths& runtime_paths,
                                                                const TickDispatchDimensions& dispatch_dimensions)
{
    if (!runtime_paths.use_raster_panel_payload)
    {
        graph.UpdateComputeDispatch(
            m_panel_payload_passes.compute,
            dispatch_dimensions.full_dispatch_x,
            dispatch_dimensions.full_dispatch_y,
            1);
    }
}

void RendererSystemFrostedGlass::UpdateTickCompositeDispatch(RendererInterface::RenderGraph& graph,
                                                             const TickRuntimePaths& runtime_paths,
                                                             const TickDispatchDimensions& dispatch_dimensions)
{
    const auto update_history_dispatch_pair =
        [&](const TemporalHistoryNodePairView& history_pair)
    {
        graph.UpdateComputeDispatch(
            history_pair.history_ab,
            dispatch_dimensions.full_dispatch_x,
            dispatch_dimensions.full_dispatch_y,
            1);
        graph.UpdateComputeDispatch(
            history_pair.history_ba,
            dispatch_dimensions.full_dispatch_x,
            dispatch_dimensions.full_dispatch_y,
            1);
    };

    if (runtime_paths.use_strict_multilayer_path)
    {
        graph.UpdateComputeDispatch(
            runtime_paths.active_back_composite_pass,
            dispatch_dimensions.full_dispatch_x,
            dispatch_dimensions.full_dispatch_y,
            1);
        UpdateTickBlurDispatch(
            graph,
            runtime_paths.multilayer_blur,
            dispatch_dimensions,
            runtime_paths.use_shared_mip_path);
        update_history_dispatch_pair(runtime_paths.active_composite.front_history);
    }
    else
    {
        update_history_dispatch_pair(runtime_paths.active_composite.single_history);
    }
}

RendererSystemFrostedGlass::TickDispatchDimensions RendererSystemFrostedGlass::BuildTickDispatchDimensions(
    unsigned width,
    unsigned height) const
{
    TickDispatchDimensions dimensions{};
    const RenderFeature::FrameDimensions frame_dimensions =
        RenderFeature::FrameDimensions::FromExtent(width, height);
    const std::pair<unsigned, unsigned> full_dispatch_group_count = frame_dimensions.ComputeGroupCount2D();
    dimensions.full_dispatch_x = full_dispatch_group_count.first;
    dimensions.full_dispatch_y = full_dispatch_group_count.second;
    dimensions.blur_dispatch_sizes = {{
        frame_dimensions.Downsampled(2u).ComputeGroupCount2D(),
        frame_dimensions.Downsampled(4u).ComputeGroupCount2D(),
        frame_dimensions.Downsampled(8u).ComputeGroupCount2D(),
        frame_dimensions.Downsampled(16u).ComputeGroupCount2D(),
        frame_dimensions.Downsampled(32u).ComputeGroupCount2D()
    }};

    return dimensions;
}

void RendererSystemFrostedGlass::UpdateTickDispatch(RendererInterface::RenderGraph& graph,
                                                    const TickExecutionPlan& execution_plan)
{
    const TickRuntimePaths& runtime_paths = execution_plan.runtime_paths;
    const bool dispatch_state_changed =
        !m_dispatch_cache_state.Matches(
            execution_plan.render_width,
            execution_plan.render_height,
            runtime_paths.use_shared_mip_path,
            runtime_paths.use_raster_panel_payload,
            runtime_paths.use_strict_multilayer_path);
    if (!dispatch_state_changed)
    {
        return;
    }

    UpdateTickBlurDispatch(
        graph,
        runtime_paths.primary_blur,
        execution_plan.dispatch_dimensions,
        runtime_paths.use_shared_mip_path);
    UpdateTickPanelPayloadDispatch(graph, runtime_paths, execution_plan.dispatch_dimensions);
    UpdateTickCompositeDispatch(graph, runtime_paths, execution_plan.dispatch_dimensions);

    m_dispatch_cache_state.Update(
        execution_plan.render_width,
        execution_plan.render_height,
        runtime_paths.use_shared_mip_path,
        runtime_paths.use_raster_panel_payload,
        runtime_paths.use_strict_multilayer_path);
}

bool RendererSystemFrostedGlass::RegisterTickBlurNodes(RendererInterface::RenderGraph& graph,
                                                       const BlurPyramidBundleView& blur_pyramid,
                                                       bool use_shared_mip_path) const
{
    for (const BlurPyramidLevelView& level : blur_pyramid.levels)
    {
        if (use_shared_mip_path)
        {
            RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, level.shared_downsample_node));
        }
        else
        {
            RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodesIfValid(
                graph,
                {level.downsample_node, level.blur_horizontal_node, level.blur_vertical_node}));
        }
    }
    return true;
}

bool RendererSystemFrostedGlass::RegisterTickPanelPayloadNodes(RendererInterface::RenderGraph& graph,
                                                               const TickRuntimePaths& runtime_paths) const
{
    if (runtime_paths.use_raster_panel_payload)
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodesIfValid(
            graph,
            {m_panel_payload_passes.raster_front, m_panel_payload_passes.raster_back}));
    }
    else
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, m_panel_payload_passes.compute));
    }
    return true;
}

bool RendererSystemFrostedGlass::RegisterTickCompositeNodes(RendererInterface::RenderGraph& graph,
                                                            const TickRuntimePaths& runtime_paths) const
{
    if (runtime_paths.use_strict_multilayer_path)
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, runtime_paths.active_back_composite_pass));
        RETURN_IF_FALSE(RegisterTickBlurNodes(graph, runtime_paths.multilayer_blur, runtime_paths.use_shared_mip_path));
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, runtime_paths.active_front_composite_pass));
    }
    else
    {
        RETURN_IF_FALSE(RenderFeature::RegisterRenderGraphNodeIfValid(graph, runtime_paths.active_single_composite_pass));
    }
    return true;
}

bool RendererSystemFrostedGlass::RegisterTickRenderGraphNodes(RendererInterface::RenderGraph& graph,
                                                              const TickExecutionPlan& execution_plan)
{
    const TickRuntimePaths& runtime_paths = execution_plan.runtime_paths;
    RETURN_IF_FALSE(RegisterTickBlurNodes(graph, runtime_paths.primary_blur, runtime_paths.use_shared_mip_path));
    RETURN_IF_FALSE(RegisterTickPanelPayloadNodes(graph, runtime_paths));
    RETURN_IF_FALSE(RegisterTickCompositeNodes(graph, runtime_paths));

    graph.RegisterRenderTargetToColorOutput(m_composite_outputs.final_output);
    return true;
}

void RendererSystemFrostedGlass::FinalizeTickHistoryState()
{
    const bool next_history_valid = m_global_params.panel_count > 0u;
    if (m_temporal_history_state.valid != next_history_valid)
    {
        m_temporal_history_state.valid = next_history_valid;
        m_need_upload_global_params = true;
    }
    m_temporal_history_state.read_is_a = !m_temporal_history_state.read_is_a;
}

bool RendererSystemFrostedGlass::Tick(RendererInterface::ResourceOperator& resource_operator,
                                      RendererInterface::RenderGraph& graph,
                                      unsigned long long interval)
{
    const float delta_seconds = static_cast<float>(interval) / 1000.0f;
    const InitDimensions dimensions = BuildInitDimensions(resource_operator);
    const InitBindings bindings = BuildInitBindings();
    const auto scene_outputs = m_scene->GetOutputs();
    RETURN_IF_FALSE(SyncStaticPassSetups(resource_operator, graph, scene_outputs, dimensions, bindings));
    const TickExecutionPlan execution_plan =
        BuildTickExecutionPlan(interval, dimensions.width, dimensions.height);
    RefreshTickPanelData(resource_operator, delta_seconds);
    UpdateTickDispatch(graph, execution_plan);
    RETURN_IF_FALSE(RegisterTickRenderGraphNodes(graph, execution_plan));
    FinalizeTickHistoryState();
    return true;
}

void RendererSystemFrostedGlass::OnResize(RendererInterface::ResourceOperator& resource_operator, unsigned width, unsigned height)
{
    (void)resource_operator;
    (void)width;
    (void)height;
    m_temporal_history_state.ResetRuntime();
    m_global_params.temporal_history_valid = 0;
    m_multilayer_over_budget_streak = 0;
    m_multilayer_cooldown_frames = 0;
    m_need_upload_global_params = true;
    m_static_pass_setup_state.Reset();
    m_dispatch_cache_state.Reset();
}

void RendererSystemFrostedGlass::DrawDebugUI()
{
    bool panel_dirty = false;
    bool global_dirty = false;

    if (ImGui::CollapsingHeader("Global Controls", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int blur_radius = static_cast<int>(m_global_params.blur_radius);
        if (ImGui::SliderInt("Blur Radius", &blur_radius, 1, 24))
        {
            m_global_params.blur_radius = static_cast<unsigned>(blur_radius);
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Response Scale", &m_global_params.blur_response_scale, 0.6f, 2.5f, "%.2f"))
        {
            global_dirty = true;
        }
        if (ImGui::SliderFloat("Blur Veil Strength", &m_global_params.blur_veil_strength, 0.0f, 2.0f, "%.2f"))
        {
            global_dirty = true;
        }
        ImGui::TextUnformatted("Full Fog Mode: On (Fixed)");
        const char* blur_source_modes[] = {"Legacy Pyramid", "Shared Mip", "Shared Dual (Fallback->SharedMip)"};
        int blur_source_mode = static_cast<int>(m_blur_source_mode);
        if (ImGui::Combo("Blur Source Mode", &blur_source_mode, blur_source_modes, IM_ARRAYSIZE(blur_source_modes)))
        {
            blur_source_mode = (std::max)(0, (std::min)(blur_source_mode, 2));
            m_blur_source_mode = static_cast<BlurSourceMode>(blur_source_mode);
            m_temporal_history_state.force_reset = true;
            m_temporal_history_state.valid = false;
            m_global_params.temporal_history_valid = 0;
            global_dirty = true;
        }
        const char* multilayer_modes[] = {"Single", "Auto", "MultiLayer"};
        int multilayer_mode = static_cast<int>(m_global_params.multilayer_mode);
        if (ImGui::Combo("Multilayer Mode", &multilayer_mode, multilayer_modes, IM_ARRAYSIZE(multilayer_modes)))
        {
            multilayer_mode = (std::max)(0, (std::min)(multilayer_mode, 2));
            m_global_params.multilayer_mode = static_cast<unsigned>(multilayer_mode);
            m_multilayer_over_budget_streak = 0;
            m_multilayer_cooldown_frames = 0;
            global_dirty = true;
        }
        if (ImGui::CollapsingHeader("Advanced / Global"))
        {
            const char* panel_payload_paths[] = {"Compute (SDF)", "Raster (Panel GBuffer)"};
            int panel_payload_path = static_cast<int>(m_panel_payload_path);
            if (ImGui::Combo("Panel Payload Path", &panel_payload_path, panel_payload_paths, IM_ARRAYSIZE(panel_payload_paths)))
            {
                panel_payload_path = (std::max)(0, (std::min)(panel_payload_path, 1));
                m_panel_payload_path = static_cast<PanelPayloadPath>(panel_payload_path);
            }
            if (ImGui::SliderFloat("Blur Kernel Sigma Scale", &m_global_params.blur_kernel_sigma_scale, 0.6f, 3.5f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Blur Sigma Normalization", &m_global_params.blur_sigma_normalization, 4.0f, 12.0f, "%.2f"))
            {
                global_dirty = true;
            }
            ImGui::TextUnformatted("Blur Detail Model: quarterMixBoost=0.50 (fixed)");
            if (ImGui::SliderFloat("Scene Edge Scale", &m_global_params.scene_edge_scale, 0.0f, 120.0f, "%.1f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Temporal History Blend", &m_global_params.temporal_history_blend, 0.0f, 0.98f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Temporal Velocity Reject", &m_global_params.temporal_reject_velocity, 0.001f, 0.20f, "%.3f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Temporal Edge Reject", &m_global_params.temporal_edge_reject, 0.0f, 2.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Multilayer Frame Budget (ms)", &m_global_params.multilayer_frame_budget_ms, 8.0f, 50.0f, "%.1f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Multilayer Overlap Threshold", &m_global_params.multilayer_overlap_threshold, 0.0f, 0.80f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Edge Power", &m_global_params.thickness_edge_power, 1.0f, 8.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Highlight Boost", &m_global_params.thickness_highlight_boost_max, 1.0f, 4.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Refraction Boost", &m_global_params.thickness_refraction_boost_max, 1.0f, 4.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Edge Shadow", &m_global_params.thickness_edge_shadow_strength, 0.0f, 1.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Range Min", &m_global_params.thickness_range_min, 0.0f, 0.10f, "%.3f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Thickness Range Max", &m_global_params.thickness_range_max, 0.01f, 0.20f, "%.3f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Edge Spec Intensity", &m_global_params.edge_spec_intensity, 0.0f, 3.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Edge Highlight Width", &m_global_params.edge_highlight_width, 0.05f, 1.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Edge Highlight White Mix", &m_global_params.edge_highlight_white_mix, 0.0f, 1.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Directional Highlight Min", &m_global_params.directional_highlight_min, 0.0f, 1.0f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Directional Highlight Max", &m_global_params.directional_highlight_max, 0.2f, 3.5f, "%.2f"))
            {
                global_dirty = true;
            }
            if (ImGui::SliderFloat("Directional Highlight Curve", &m_global_params.directional_highlight_curve, 0.5f, 4.0f, "%.2f"))
            {
                global_dirty = true;
            }
            ImGui::TextUnformatted("NaN Probe: Off (Fixed)");
        }
        if (ImGui::Button("Reset Temporal History"))
        {
            m_temporal_history_state.force_reset = true;
            m_temporal_history_state.valid = false;
            m_global_params.temporal_history_valid = 0;
            global_dirty = true;
        }
    }
    if (ImGui::CollapsingHeader("Runtime Summary", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Temporal History: %s | Read Buffer: %s",
                    m_temporal_history_state.valid ? "Valid" : "Invalid",
                    m_temporal_history_state.read_is_a ? "A" : "B");
        const unsigned requested_blur_source_mode = static_cast<unsigned>(m_blur_source_mode);
        const char* requested_blur_source_label = "Unknown";
        if (requested_blur_source_mode == BLUR_SOURCE_MODE_LEGACY_PYRAMID)
        {
            requested_blur_source_label = "Legacy Pyramid";
        }
        else if (requested_blur_source_mode == BLUR_SOURCE_MODE_SHARED_MIP)
        {
            requested_blur_source_label = "Shared Mip";
        }
        else if (requested_blur_source_mode == BLUR_SOURCE_MODE_SHARED_DUAL)
        {
            requested_blur_source_label = "Shared Dual";
        }

        const char* runtime_blur_source_label = "Unknown";
        if (m_global_params.blur_source_mode == BLUR_SOURCE_MODE_LEGACY_PYRAMID)
        {
            runtime_blur_source_label = "Legacy Pyramid";
        }
        else if (m_global_params.blur_source_mode == BLUR_SOURCE_MODE_SHARED_MIP)
        {
            runtime_blur_source_label = "Shared Mip";
        }
        else if (m_global_params.blur_source_mode == BLUR_SOURCE_MODE_SHARED_DUAL)
        {
            runtime_blur_source_label = "Shared Dual";
        }
        ImGui::Text("Blur Source: Requested=%s | Runtime=%s", requested_blur_source_label, runtime_blur_source_label);
        ImGui::TextUnformatted("Full Fog Mode: On (Fixed)");
        ImGui::Text("Multilayer Runtime: %s | Cooldown: %u | OverBudgetStreak: %u",
                    m_multilayer_runtime_enabled ? "Enabled" : "Disabled",
                    m_multilayer_cooldown_frames,
                    m_multilayer_over_budget_streak);
        const bool strict_multilayer_path_active =
            m_global_params.multilayer_mode == MULTILAYER_MODE_FORCE &&
            m_multilayer_runtime_enabled;
        ImGui::Text("Multilayer Path: %s",
                    strict_multilayer_path_active ? "Strict Sequential (Back->Front)" : "Fast Single-Pass");
        const bool using_raster_panel_payload = m_panel_payload_path == PanelPayloadPath::RasterPanelGBuffer;
        ImGui::Text("Panel Payload Path: %s",
                    using_raster_panel_payload ? "Raster (Panel GBuffer)" : "Compute (SDF)");
        const char* runtime_payload_path =
            m_last_runtime_used_raster_payload ? "Raster (Panel GBuffer)" : "Compute (SDF)";
        const char* runtime_composite_path =
            m_last_runtime_used_strict_multilayer ? "Strict Sequential (Back->Front)" : "Fast Single-Pass";
        const char* runtime_blur_path =
            m_last_runtime_used_shared_mip_path ? "SharedMip" : "LegacyPyramid";
        ImGui::Text("Frosted Active Nodes (expected): %u", m_last_expected_registered_pass_count);
        ImGui::Text("Runtime Path: blur=%s | payload=%s | composite=%s",
                    runtime_blur_path,
                    runtime_payload_path,
                    runtime_composite_path);
        const unsigned internal_panel_count = static_cast<unsigned>(m_panel_descs.size());
        const unsigned external_manual_world_panel_count = static_cast<unsigned>(m_external_world_space_panel_descs.size());
        const unsigned external_manual_overlay_panel_count = static_cast<unsigned>(m_external_overlay_panel_descs.size());
        const unsigned external_producer_world_panel_count = static_cast<unsigned>(m_producer_world_space_panel_descs.size());
        const unsigned external_producer_overlay_panel_count = static_cast<unsigned>(m_producer_overlay_panel_descs.size());
        ImGui::Text("Panel Sources: internal=%u | extW(manual/prod)=%u/%u extO(manual/prod)=%u/%u | uploaded=%u/%u",
                    internal_panel_count,
                    external_manual_world_panel_count,
                    external_producer_world_panel_count,
                    external_manual_overlay_panel_count,
                    external_producer_overlay_panel_count,
                    m_global_params.panel_count,
                    m_last_upload_requested_panel_count);
        const auto& highlight_light = m_global_params.highlight_light_dir_weight;
        ImGui::Text("Highlight Light: %s | Dir: (%.2f, %.2f, %.2f)",
                    highlight_light.w > 0.5f ? "Directional" : "Fallback",
                    highlight_light.x,
                    highlight_light.y,
                    highlight_light.z);
        ImGui::TextUnformatted("NaN Probe Legend: Red=Scene, Green=Payload, Blue=Blur, Yellow=History, Magenta=Velocity (disabled)");
        if (m_panel_payload_compute_fallback_active)
        {
            ImGui::TextUnformatted("Panel Payload Runtime: Raster requested but not ready; using Compute fallback.");
        }
    }

    const unsigned internal_panel_count = static_cast<unsigned>(m_panel_descs.size());
    const unsigned external_manual_world_panel_count = static_cast<unsigned>(m_external_world_space_panel_descs.size());
    const unsigned external_manual_overlay_panel_count = static_cast<unsigned>(m_external_overlay_panel_descs.size());
    const unsigned external_producer_world_panel_count = static_cast<unsigned>(m_producer_world_space_panel_descs.size());
    const unsigned external_producer_overlay_panel_count = static_cast<unsigned>(m_producer_overlay_panel_descs.size());
    const unsigned total_debug_panel_count =
        internal_panel_count +
        external_producer_world_panel_count +
        external_producer_overlay_panel_count +
        external_manual_world_panel_count +
        external_manual_overlay_panel_count;
    if (total_debug_panel_count == 0)
    {
        ImGui::TextUnformatted("No frosted panels.");
        if (panel_dirty)
        {
            m_need_upload_panels = true;
        }
        if (global_dirty)
        {
            m_need_upload_global_params = true;
        }
        return;
    }

    if (ImGui::CollapsingHeader("Selected Panel"))
    {
        int selected_panel_index = static_cast<int>(m_debug_selected_panel_index);
        const int max_panel_index = static_cast<int>(total_debug_panel_count) - 1;
        selected_panel_index = (std::max)(0, (std::min)(selected_panel_index, max_panel_index));
        if (ImGui::SliderInt("Panel Index", &selected_panel_index, 0, max_panel_index))
        {
            m_debug_selected_panel_index = static_cast<unsigned>(selected_panel_index);
        }
        else
        {
            m_debug_selected_panel_index = static_cast<unsigned>(selected_panel_index);
        }

        FrostedGlassPanelDesc* selected_panel = nullptr;
        DebugPanelSource selected_panel_source_type = DebugPanelSource::Internal;
        const char* selected_panel_source = "Unknown";
        unsigned selected_panel_local_index = 0;
        unsigned panel_index_cursor = m_debug_selected_panel_index;

        if (panel_index_cursor < m_panel_descs.size())
        {
            selected_panel = &m_panel_descs[panel_index_cursor];
            selected_panel_source_type = DebugPanelSource::Internal;
            selected_panel_source = "Internal (Editable)";
            selected_panel_local_index = panel_index_cursor;
        }
        else
        {
            panel_index_cursor -= static_cast<unsigned>(m_panel_descs.size());
            if (panel_index_cursor < m_producer_world_space_panel_descs.size())
            {
                selected_panel = &m_producer_world_space_panel_descs[panel_index_cursor];
                selected_panel_source_type = DebugPanelSource::ProducerWorld;
                selected_panel_source = "Producer World (Debug Override)";
                selected_panel_local_index = panel_index_cursor;
            }
            else
            {
                panel_index_cursor -= static_cast<unsigned>(m_producer_world_space_panel_descs.size());
                if (panel_index_cursor < m_producer_overlay_panel_descs.size())
                {
                    selected_panel = &m_producer_overlay_panel_descs[panel_index_cursor];
                    selected_panel_source_type = DebugPanelSource::ProducerOverlay;
                    selected_panel_source = "Producer Overlay (Debug Override)";
                    selected_panel_local_index = panel_index_cursor;
                }
                else
                {
                    panel_index_cursor -= static_cast<unsigned>(m_producer_overlay_panel_descs.size());
                    if (panel_index_cursor < m_external_world_space_panel_descs.size())
                    {
                        selected_panel = &m_external_world_space_panel_descs[panel_index_cursor];
                        selected_panel_source_type = DebugPanelSource::ManualWorld;
                        selected_panel_source = "Manual World (Debug Override)";
                        selected_panel_local_index = panel_index_cursor;
                    }
                    else
                    {
                        panel_index_cursor -= static_cast<unsigned>(m_external_world_space_panel_descs.size());
                        if (panel_index_cursor < m_external_overlay_panel_descs.size())
                        {
                            selected_panel = &m_external_overlay_panel_descs[panel_index_cursor];
                            selected_panel_source_type = DebugPanelSource::ManualOverlay;
                            selected_panel_source = "Manual Overlay (Debug Override)";
                            selected_panel_local_index = panel_index_cursor;
                        }
                    }
                }
            }
        }

        if (selected_panel == nullptr)
        {
            ImGui::TextUnformatted("Selected panel index is out of range.");
        }
        else
        {
            ImGui::Text("Selected Panel Source: %s | Local Index: %u", selected_panel_source, selected_panel_local_index);
            if (selected_panel_source_type != DebugPanelSource::Internal)
            {
                ImGui::TextUnformatted("External/producer panel edits are stored as Frosted debug overrides.");
            }

            float editable_panel_max_corner_radius = 0.0f;
            auto& panel = *selected_panel;

            if (ImGui::SliderFloat2("Center UV", &panel.center_uv.x, 0.0f, 1.0f, "%.3f"))
            {
                panel_dirty = true;
            }
            if (ImGui::SliderFloat2("Half Size UV", &panel.half_size_uv.x, 0.02f, 0.48f, "%.3f"))
            {
                panel_dirty = true;
            }

            int shape_type = static_cast<int>(panel.shape_type);
            const char* shape_types[] = {"RoundedRect", "Circle", "ShapeMask"};
            if (ImGui::Combo("Shape Type", &shape_type, shape_types, IM_ARRAYSIZE(shape_types)))
            {
                panel.shape_type = static_cast<PanelShapeType>(shape_type);
                panel_dirty = true;
            }
            if (ImGui::SliderFloat("Layer Order", &panel.layer_order, -8.0f, 8.0f, "%.1f"))
            {
                panel_dirty = true;
            }
            bool world_space_mode = panel.world_space_mode != 0;
            if (ImGui::Checkbox("World Space Panel", &world_space_mode))
            {
                panel.world_space_mode = world_space_mode ? 1u : 0u;
                if (world_space_mode)
                {
                    panel.depth_policy = PanelDepthPolicy::SceneOcclusion;
                }
                panel_dirty = true;
            }
            const char* panel_depth_policies[] = {"Overlay", "Scene Occlusion"};
            int panel_depth_policy = static_cast<int>(panel.depth_policy);
            if (ImGui::Combo("Panel Depth Policy", &panel_depth_policy, panel_depth_policies, IM_ARRAYSIZE(panel_depth_policies)))
            {
                panel_depth_policy = (std::max)(0, (std::min)(panel_depth_policy, 1));
                panel.depth_policy = static_cast<PanelDepthPolicy>(panel_depth_policy);
                panel_dirty = true;
            }
            if (world_space_mode)
            {
                if (ImGui::DragFloat3("World Center", &panel.world_center.x, 0.01f, -20.0f, 20.0f, "%.3f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::DragFloat3("World Axis U", &panel.world_axis_u.x, 0.01f, -5.0f, 5.0f, "%.3f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::DragFloat3("World Axis V", &panel.world_axis_v.x, 0.01f, -5.0f, 5.0f, "%.3f"))
                {
                    panel_dirty = true;
                }
            }

            const char* interaction_states[] = {"Idle", "Hover", "Grab", "Move", "Scale"};
            int interaction_state = static_cast<int>(panel.interaction_state);
            if (ImGui::Combo("Interaction State", &interaction_state, interaction_states, IM_ARRAYSIZE(interaction_states)))
            {
                interaction_state = (std::max)(0, (std::min)(interaction_state, static_cast<int>(PanelInteractionState::Count) - 1));
                panel.interaction_state = static_cast<PanelInteractionState>(interaction_state);
                panel_dirty = true;
            }
            if (ImGui::SliderFloat("Panel Alpha", &panel.panel_alpha, 0.0f, 1.0f, "%.2f"))
            {
                panel_dirty = true;
            }

            const float max_corner_radius = (std::max)(0.0f, (std::min)(panel.half_size_uv.x, panel.half_size_uv.y) - 0.001f);
            editable_panel_max_corner_radius = max_corner_radius;
            if (ImGui::SliderFloat("Blur Sigma", &panel.blur_sigma, 0.2f, 24.0f, "%.2f"))
            {
                panel_dirty = true;
            }
            if (ImGui::SliderFloat("Blur Strength", &panel.blur_strength, 0.0f, 1.0f, "%.2f"))
            {
                panel_dirty = true;
            }
            if (ImGui::ColorEdit3("Tint Color", &panel.tint_color.x))
            {
                panel_dirty = true;
            }
            if (ImGui::SliderFloat("Fresnel Intensity", &panel.fresnel_intensity, 0.0f, 0.6f, "%.3f"))
            {
                panel_dirty = true;
            }
            if (ImGui::CollapsingHeader("Advanced / Panel"))
            {
                if (ImGui::SliderFloat("Custom Shape Index", &panel.custom_shape_index, 0.0f, 7.0f, "%.1f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("State Transition Speed", &panel.interaction_transition_speed, 1.0f, 24.0f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Corner Radius", &panel.corner_radius, 0.0f, max_corner_radius, "%.3f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Rim Intensity", &panel.rim_intensity, 0.0f, 0.6f, "%.3f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Edge Softness", &panel.edge_softness, 0.1f, 4.0f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Thickness", &panel.thickness, 0.0f, 0.10f, "%.3f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Refraction Strength", &panel.refraction_strength, 0.0f, 4.0f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Fresnel Power", &panel.fresnel_power, 1.0f, 10.0f, "%.2f"))
                {
                    panel_dirty = true;
                }

                int curve_state = static_cast<int>(m_debug_selected_curve_state_index);
                curve_state = (std::max)(0, (std::min)(curve_state, static_cast<int>(PanelInteractionState::Count) - 1));
                if (ImGui::Combo("Edit State Curve", &curve_state, interaction_states, IM_ARRAYSIZE(interaction_states)))
                {
                    m_debug_selected_curve_state_index = static_cast<unsigned>(curve_state);
                }
                else
                {
                    m_debug_selected_curve_state_index = static_cast<unsigned>(curve_state);
                }
                auto& curve = panel.state_curves[m_debug_selected_curve_state_index];
                if (ImGui::SliderFloat("Curve Blur Sigma Scale", &curve.blur_sigma_scale, 0.4f, 2.5f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Curve Blur Strength Scale", &curve.blur_strength_scale, 0.4f, 2.5f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Curve Rim Scale", &curve.rim_intensity_scale, 0.4f, 2.5f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Curve Fresnel Scale", &curve.fresnel_intensity_scale, 0.4f, 2.5f, "%.2f"))
                {
                    panel_dirty = true;
                }
                if (ImGui::SliderFloat("Curve Alpha Scale", &curve.alpha_scale, 0.0f, 1.5f, "%.2f"))
                {
                    panel_dirty = true;
                }
            }

            if (panel_dirty)
            {
                const auto clamp = [](float value, float min_value, float max_value) -> float
                {
                    return (std::max)(min_value, (std::min)(value, max_value));
                };

                const float center_min_x = panel.half_size_uv.x;
                const float center_min_y = panel.half_size_uv.y;
                const float center_max_x = 1.0f - panel.half_size_uv.x;
                const float center_max_y = 1.0f - panel.half_size_uv.y;
                panel.center_uv.x = clamp(panel.center_uv.x, center_min_x, center_max_x);
                panel.center_uv.y = clamp(panel.center_uv.y, center_min_y, center_max_y);
                panel.corner_radius = clamp(panel.corner_radius, 0.0f, editable_panel_max_corner_radius);
                panel.blur_sigma = clamp(panel.blur_sigma, 0.2f, 24.0f);
                panel.blur_strength = clamp(panel.blur_strength, 0.0f, 1.0f);
                panel.rim_intensity = clamp(panel.rim_intensity, 0.0f, 1.0f);
                panel.edge_softness = clamp(panel.edge_softness, 0.1f, 8.0f);
                panel.thickness = clamp(panel.thickness, 0.0f, 0.2f);
                panel.refraction_strength = clamp(panel.refraction_strength, 0.0f, 8.0f);
                panel.fresnel_intensity = clamp(panel.fresnel_intensity, 0.0f, 1.0f);
                panel.fresnel_power = clamp(panel.fresnel_power, 1.0f, 16.0f);
                panel.custom_shape_index = clamp(panel.custom_shape_index, 0.0f, 7.0f);
                panel.layer_order = clamp(panel.layer_order, -8.0f, 8.0f);
                panel.panel_alpha = clamp(panel.panel_alpha, 0.0f, 1.0f);
                panel.interaction_transition_speed = clamp(panel.interaction_transition_speed, 1.0f, 24.0f);
                panel.world_space_mode = panel.world_space_mode == 0 ? 0u : 1u;
                panel.depth_policy =
                    panel.depth_policy == PanelDepthPolicy::SceneOcclusion
                        ? PanelDepthPolicy::SceneOcclusion
                        : PanelDepthPolicy::Overlay;
                const auto length_sq = [](const glm::fvec3& value) -> float
                {
                    return value.x * value.x + value.y * value.y + value.z * value.z;
                };
                if (length_sq(panel.world_axis_u) < 1e-6f)
                {
                    panel.world_axis_u = glm::fvec3(0.70f, 0.00f, 0.00f);
                }
                if (length_sq(panel.world_axis_v) < 1e-6f)
                {
                    panel.world_axis_v = glm::fvec3(0.00f, 0.45f, 0.00f);
                }
                for (auto& state_curve : panel.state_curves)
                {
                    state_curve.blur_sigma_scale = clamp(state_curve.blur_sigma_scale, 0.4f, 2.5f);
                    state_curve.blur_strength_scale = clamp(state_curve.blur_strength_scale, 0.4f, 2.5f);
                    state_curve.rim_intensity_scale = clamp(state_curve.rim_intensity_scale, 0.4f, 2.5f);
                    state_curve.fresnel_intensity_scale = clamp(state_curve.fresnel_intensity_scale, 0.4f, 2.5f);
                    state_curve.alpha_scale = clamp(state_curve.alpha_scale, 0.0f, 1.5f);
                }
                SaveDebugPanelOverride(selected_panel_source_type, selected_panel_local_index, panel);
                m_need_upload_panels = true;
            }
        }
    }
    if (global_dirty)
    {
        const auto clamp = [](float value, float min_value, float max_value) -> float
        {
            return (std::max)(min_value, (std::min)(value, max_value));
        };
        m_global_params.temporal_history_blend = clamp(m_global_params.temporal_history_blend, 0.0f, 0.98f);
        m_global_params.temporal_reject_velocity = clamp(m_global_params.temporal_reject_velocity, 0.001f, 0.20f);
        m_global_params.temporal_edge_reject = clamp(m_global_params.temporal_edge_reject, 0.0f, 2.0f);
        m_global_params.blur_kernel_sigma_scale = clamp(m_global_params.blur_kernel_sigma_scale, 0.6f, 3.5f);
        m_global_params.blur_response_scale = clamp(m_global_params.blur_response_scale, 0.6f, 2.5f);
        m_global_params.blur_sigma_normalization = clamp(m_global_params.blur_sigma_normalization, 4.0f, 12.0f);
        m_global_params.blur_veil_strength = clamp(m_global_params.blur_veil_strength, 0.0f, 2.0f);
        m_global_params.multilayer_frame_budget_ms = clamp(m_global_params.multilayer_frame_budget_ms, 8.0f, 50.0f);
        m_global_params.multilayer_overlap_threshold = clamp(m_global_params.multilayer_overlap_threshold, 0.0f, 0.80f);
        m_global_params.thickness_edge_power = clamp(m_global_params.thickness_edge_power, 1.0f, 8.0f);
        m_global_params.thickness_highlight_boost_max = clamp(m_global_params.thickness_highlight_boost_max, 1.0f, 4.0f);
        m_global_params.thickness_refraction_boost_max = clamp(m_global_params.thickness_refraction_boost_max, 1.0f, 4.0f);
        m_global_params.thickness_edge_shadow_strength = clamp(m_global_params.thickness_edge_shadow_strength, 0.0f, 1.0f);
        m_global_params.thickness_range_min = clamp(m_global_params.thickness_range_min, 0.0f, 0.10f);
        m_global_params.thickness_range_max = clamp(m_global_params.thickness_range_max, 0.01f, 0.20f);
        m_global_params.edge_spec_intensity = clamp(m_global_params.edge_spec_intensity, 0.0f, 3.0f);
        m_global_params.edge_highlight_width = clamp(m_global_params.edge_highlight_width, 0.05f, 1.0f);
        m_global_params.edge_highlight_white_mix = clamp(m_global_params.edge_highlight_white_mix, 0.0f, 1.0f);
        m_global_params.directional_highlight_min = clamp(m_global_params.directional_highlight_min, 0.0f, 1.0f);
        m_global_params.directional_highlight_max = clamp(m_global_params.directional_highlight_max, 0.2f, 3.5f);
        m_global_params.directional_highlight_curve = clamp(m_global_params.directional_highlight_curve, 0.5f, 4.0f);
        if (m_global_params.directional_highlight_max < m_global_params.directional_highlight_min + 0.05f)
        {
            m_global_params.directional_highlight_max = m_global_params.directional_highlight_min + 0.05f;
        }
        if (m_global_params.thickness_range_max < m_global_params.thickness_range_min + 0.001f)
        {
            m_global_params.thickness_range_max = m_global_params.thickness_range_min + 0.001f;
        }
        m_blur_source_mode = static_cast<BlurSourceMode>((std::max)(0, (std::min)(static_cast<int>(m_blur_source_mode), 2)));
        m_global_params.blur_source_mode = static_cast<unsigned>(m_blur_source_mode);
        m_global_params.multilayer_mode = static_cast<unsigned>((std::max)(0, (std::min)(static_cast<int>(m_global_params.multilayer_mode), 2)));
        m_global_params.nan_debug_mode = 0u;
        m_need_upload_global_params = true;
    }
}

void RendererSystemFrostedGlass::UpdatePanelRuntimeStates(float delta_seconds)
{
    if (m_panel_runtime_states.size() != m_panel_descs.size())
    {
        m_panel_runtime_states.resize(m_panel_descs.size());
        for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
        {
            auto& runtime_state = m_panel_runtime_states[panel_index];
            runtime_state.state_weights.fill(0.0f);
            runtime_state.target_state = m_panel_descs[panel_index].interaction_state;
            runtime_state.state_weights[ToInteractionStateIndex(runtime_state.target_state)] = 1.0f;
        }
        m_need_upload_panels = true;
    }

    const float safe_delta_seconds = (std::max)(0.0f, delta_seconds);
    bool state_weight_dirty = false;
    for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
    {
        const auto& panel_desc = m_panel_descs[panel_index];
        auto& runtime_state = m_panel_runtime_states[panel_index];
        runtime_state.target_state = panel_desc.interaction_state;

        const unsigned target_state_index = ToInteractionStateIndex(runtime_state.target_state);
        const float transition_speed = (std::max)(panel_desc.interaction_transition_speed, 1.0f);
        const float blend_alpha = 1.0f - std::exp(-transition_speed * safe_delta_seconds);

        float total_weight = 0.0f;
        for (unsigned state_index = 0; state_index < PANEL_INTERACTION_STATE_COUNT; ++state_index)
        {
            const float target_weight = state_index == target_state_index ? 1.0f : 0.0f;
            const float current_weight = runtime_state.state_weights[state_index];
            const float updated_weight = current_weight + (target_weight - current_weight) * blend_alpha;
            if (std::abs(updated_weight - current_weight) > 1e-4f)
            {
                state_weight_dirty = true;
            }
            runtime_state.state_weights[state_index] = updated_weight;
            total_weight += updated_weight;
        }

        if (total_weight > 1e-6f)
        {
            for (auto& weight : runtime_state.state_weights)
            {
                weight /= total_weight;
            }
        }
        else
        {
            runtime_state.state_weights.fill(0.0f);
            runtime_state.state_weights[target_state_index] = 1.0f;
            state_weight_dirty = true;
        }
    }

    if (state_weight_dirty)
    {
        m_need_upload_panels = true;
    }
}

void RendererSystemFrostedGlass::RefreshExternalPanelsFromProducers()
{
    if (m_external_panel_producers.empty())
    {
        if (!m_producer_world_space_panel_descs.empty() || !m_producer_overlay_panel_descs.empty())
        {
            m_producer_world_space_panel_descs.clear();
            m_producer_overlay_panel_descs.clear();
            m_need_upload_panels = true;
        }
        return;
    }

    std::vector<FrostedGlassPanelDesc> world_space_panels;
    std::vector<FrostedGlassPanelDesc> overlay_panels;
    world_space_panels.reserve(m_producer_world_space_panel_descs.size());
    overlay_panels.reserve(m_producer_overlay_panel_descs.size());
    for (const auto& producer_entry : m_external_panel_producers)
    {
        if (!producer_entry.producer)
        {
            continue;
        }
        producer_entry.producer(world_space_panels, overlay_panels);
    }

    for (auto& panel_desc : world_space_panels)
    {
        panel_desc.world_space_mode = 1u;
    }
    for (auto& panel_desc : overlay_panels)
    {
        panel_desc.world_space_mode = 0u;
        panel_desc.depth_policy = PanelDepthPolicy::Overlay;
    }

    m_producer_world_space_panel_descs = std::move(world_space_panels);
    m_producer_overlay_panel_descs = std::move(overlay_panels);
    m_need_upload_panels = true;
}

RendererSystemFrostedGlass::PanelStateCurve RendererSystemFrostedGlass::GetBlendedStateCurve(unsigned panel_index) const
{
    PanelStateCurve blended_state_curve{};
    blended_state_curve.blur_sigma_scale = 0.0f;
    blended_state_curve.blur_strength_scale = 0.0f;
    blended_state_curve.rim_intensity_scale = 0.0f;
    blended_state_curve.fresnel_intensity_scale = 0.0f;
    blended_state_curve.alpha_scale = 0.0f;

    GLTF_CHECK(panel_index < m_panel_descs.size());
    GLTF_CHECK(panel_index < m_panel_runtime_states.size());
    const auto& panel_desc = m_panel_descs[panel_index];
    const auto& runtime_state = m_panel_runtime_states[panel_index];

    float total_weight = 0.0f;
    for (unsigned state_index = 0; state_index < PANEL_INTERACTION_STATE_COUNT; ++state_index)
    {
        const float weight = (std::max)(0.0f, runtime_state.state_weights[state_index]);
        const auto& state_curve = panel_desc.state_curves[state_index];
        blended_state_curve.blur_sigma_scale += state_curve.blur_sigma_scale * weight;
        blended_state_curve.blur_strength_scale += state_curve.blur_strength_scale * weight;
        blended_state_curve.rim_intensity_scale += state_curve.rim_intensity_scale * weight;
        blended_state_curve.fresnel_intensity_scale += state_curve.fresnel_intensity_scale * weight;
        blended_state_curve.alpha_scale += state_curve.alpha_scale * weight;
        total_weight += weight;
    }

    if (total_weight <= 1e-6f)
    {
        return panel_desc.state_curves[ToInteractionStateIndex(panel_desc.interaction_state)];
    }

    const float inv_total_weight = 1.0f / total_weight;
    blended_state_curve.blur_sigma_scale *= inv_total_weight;
    blended_state_curve.blur_strength_scale *= inv_total_weight;
    blended_state_curve.rim_intensity_scale *= inv_total_weight;
    blended_state_curve.fresnel_intensity_scale *= inv_total_weight;
    blended_state_curve.alpha_scale *= inv_total_weight;
    return blended_state_curve;
}

void RendererSystemFrostedGlass::UploadPanelData(RendererInterface::ResourceOperator& resource_operator)
{
    if (!m_need_upload_panels && !m_need_upload_global_params)
    {
        return;
    }

    if (m_need_upload_panels)
    {
        m_last_upload_requested_panel_count = static_cast<unsigned>(
            m_panel_descs.size() +
            m_producer_world_space_panel_descs.size() +
            m_producer_overlay_panel_descs.size() +
            m_external_world_space_panel_descs.size() +
            m_external_overlay_panel_descs.size());
        std::vector<FrostedGlassPanelGpuData> panel_gpu_datas;
        panel_gpu_datas.reserve((std::min)(
            static_cast<unsigned>(MAX_PANEL_COUNT),
            m_last_upload_requested_panel_count));

        const PanelStateCurve identity_state_curve{};
        auto append_panel_gpu_data = [&](const FrostedGlassPanelDesc& panel_desc, const PanelStateCurve& blended_state_curve)
        {
            if (panel_gpu_datas.size() >= MAX_PANEL_COUNT)
            {
                return;
            }
            panel_gpu_datas.push_back(ConvertPanelToGpuData(panel_desc, blended_state_curve));
        };

        for (unsigned panel_index = 0; panel_index < m_panel_descs.size(); ++panel_index)
        {
            const auto blended_state_curve = GetBlendedStateCurve(panel_index);
            append_panel_gpu_data(m_panel_descs[panel_index], blended_state_curve);
        }
        for (const auto& producer_world_panel : m_producer_world_space_panel_descs)
        {
            FrostedGlassPanelDesc normalized_panel = producer_world_panel;
            normalized_panel.world_space_mode = 1u;
            append_panel_gpu_data(normalized_panel, identity_state_curve);
        }
        for (const auto& producer_overlay_panel : m_producer_overlay_panel_descs)
        {
            FrostedGlassPanelDesc normalized_panel = producer_overlay_panel;
            normalized_panel.world_space_mode = 0u;
            normalized_panel.depth_policy = PanelDepthPolicy::Overlay;
            append_panel_gpu_data(normalized_panel, identity_state_curve);
        }
        for (const auto& external_world_panel : m_external_world_space_panel_descs)
        {
            FrostedGlassPanelDesc normalized_panel = external_world_panel;
            normalized_panel.world_space_mode = 1u;
            append_panel_gpu_data(normalized_panel, identity_state_curve);
        }
        for (const auto& external_overlay_panel : m_external_overlay_panel_descs)
        {
            FrostedGlassPanelDesc normalized_panel = external_overlay_panel;
            normalized_panel.world_space_mode = 0u;
            normalized_panel.depth_policy = PanelDepthPolicy::Overlay;
            append_panel_gpu_data(normalized_panel, identity_state_curve);
        }

        if (!panel_gpu_datas.empty())
        {
            RendererInterface::BufferUploadDesc panel_data_upload_desc{};
            panel_data_upload_desc.data = panel_gpu_datas.data();
            panel_data_upload_desc.size = panel_gpu_datas.size() * sizeof(FrostedGlassPanelGpuData);
            resource_operator.UploadBufferData(m_frosted_panel_data_handle, panel_data_upload_desc);
        }

        m_global_params.panel_count = static_cast<unsigned>(panel_gpu_datas.size());
        m_need_upload_global_params = true;
    }

    if (m_need_upload_global_params)
    {
        RendererInterface::BufferUploadDesc global_params_upload_desc{};
        global_params_upload_desc.data = &m_global_params;
        global_params_upload_desc.size = sizeof(FrostedGlassGlobalParams);
        resource_operator.UploadBufferData(m_frosted_global_params_handle, global_params_upload_desc);
    }

    m_need_upload_panels = false;
    m_need_upload_global_params = false;
}

RendererSystemFrostedGlass::FrostedGlassPanelGpuData RendererSystemFrostedGlass::ConvertPanelToGpuData(
    const FrostedGlassPanelDesc& panel_desc, const PanelStateCurve& blended_state_curve) const
{
    const auto clamp = [](float value, float min_value, float max_value) -> float
    {
        return (std::max)(min_value, (std::min)(value, max_value));
    };

    const float dynamic_blur_sigma = clamp(
        panel_desc.blur_sigma * (std::max)(0.0f, blended_state_curve.blur_sigma_scale),
        0.2f,
        24.0f);
    const float dynamic_blur_strength = clamp(
        panel_desc.blur_strength * (std::max)(0.0f, blended_state_curve.blur_strength_scale),
        0.0f,
        1.0f);
    const float dynamic_rim_intensity = clamp(
        panel_desc.rim_intensity * (std::max)(0.0f, blended_state_curve.rim_intensity_scale),
        0.0f,
        1.0f);
    const float dynamic_fresnel_intensity = clamp(
        panel_desc.fresnel_intensity * (std::max)(0.0f, blended_state_curve.fresnel_intensity_scale),
        0.0f,
        1.0f);
    const float dynamic_panel_alpha = clamp(
        panel_desc.panel_alpha * (std::max)(0.0f, blended_state_curve.alpha_scale),
        0.0f,
        1.0f);

    FrostedGlassPanelGpuData gpu_data{};
    gpu_data.center_half_size = {
        panel_desc.center_uv.x,
        panel_desc.center_uv.y,
        panel_desc.half_size_uv.x,
        panel_desc.half_size_uv.y
    };
    gpu_data.world_center_mode = {
        panel_desc.world_center.x,
        panel_desc.world_center.y,
        panel_desc.world_center.z,
        panel_desc.world_space_mode == 0 ? 0.0f : 1.0f
    };
    gpu_data.world_axis_u = {
        panel_desc.world_axis_u.x,
        panel_desc.world_axis_u.y,
        panel_desc.world_axis_u.z,
        0.0f
    };
    gpu_data.world_axis_v = {
        panel_desc.world_axis_v.x,
        panel_desc.world_axis_v.y,
        panel_desc.world_axis_v.z,
        0.0f
    };
    gpu_data.corner_blur_rim = {
        panel_desc.corner_radius,
        dynamic_blur_sigma,
        dynamic_blur_strength,
        dynamic_rim_intensity
    };
    gpu_data.tint_depth_weight = {
        panel_desc.tint_color.x,
        panel_desc.tint_color.y,
        panel_desc.tint_color.z,
        0.0f
    };
    gpu_data.shape_info = {
        static_cast<float>(panel_desc.shape_type),
        panel_desc.edge_softness,
        panel_desc.custom_shape_index,
        dynamic_panel_alpha
    };
    gpu_data.optical_info = {
        panel_desc.thickness,
        panel_desc.refraction_strength,
        dynamic_fresnel_intensity,
        panel_desc.fresnel_power
    };
    gpu_data.layering_info = {
        panel_desc.layer_order,
        panel_desc.depth_policy == PanelDepthPolicy::SceneOcclusion ? 1.0f : 0.0f,
        0.0f,
        0.0f
    };
    return gpu_data;
}



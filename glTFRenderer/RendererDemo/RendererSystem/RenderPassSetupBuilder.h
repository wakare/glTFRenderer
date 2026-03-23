#pragma once

#include "RendererInterface.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RenderFeature
{
    struct FrameDimensions
    {
        unsigned width{1};
        unsigned height{1};

        static FrameDimensions FromExtent(unsigned width, unsigned height)
        {
            return {
                .width = width == 0 ? 1u : width,
                .height = height == 0 ? 1u : height
            };
        }

        static FrameDimensions FromResourceOperator(RendererInterface::ResourceOperator& resource_operator)
        {
            return FromExtent(
                resource_operator.GetCurrentRenderWidth(),
                resource_operator.GetCurrentRenderHeight());
        }

        FrameDimensions Downsampled(unsigned divisor) const
        {
            const unsigned clamped_divisor = divisor == 0 ? 1u : divisor;
            return FromExtent(
                (width + clamped_divisor - 1u) / clamped_divisor,
                (height + clamped_divisor - 1u) / clamped_divisor);
        }

        std::pair<unsigned, unsigned> ComputeGroupCount2D(
            unsigned group_size_x = 8,
            unsigned group_size_y = 8) const
        {
            const unsigned clamped_group_size_x = group_size_x == 0 ? 1u : group_size_x;
            const unsigned clamped_group_size_y = group_size_y == 0 ? 1u : group_size_y;
            return {
                (width + clamped_group_size_x - 1u) / clamped_group_size_x,
                (height + clamped_group_size_y - 1u) / clamped_group_size_y
            };
        }

        RendererInterface::RenderExecuteCommand MakeDispatch2D(
            unsigned group_size_x = 8,
            unsigned group_size_y = 8,
            unsigned group_size_z = 1) const
        {
            const std::pair<unsigned, unsigned> group_count = ComputeGroupCount2D(group_size_x, group_size_y);
            RendererInterface::RenderExecuteCommand dispatch_command{};
            dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
            dispatch_command.parameter.dispatch_parameter.group_size_x = group_count.first;
            dispatch_command.parameter.dispatch_parameter.group_size_y = group_count.second;
            dispatch_command.parameter.dispatch_parameter.group_size_z = group_size_z;
            return dispatch_command;
        }

        RendererInterface::RenderExecuteCommand MakeTraceRays2D(unsigned dispatch_depth = 1) const
        {
            RendererInterface::RenderExecuteCommand trace_command{};
            trace_command.type = RendererInterface::ExecuteCommandType::RAY_TRACING_COMMAND;
            trace_command.parameter.ray_tracing_dispatch_parameter.dispatch_width = width;
            trace_command.parameter.ray_tracing_dispatch_parameter.dispatch_height = height;
            trace_command.parameter.ray_tracing_dispatch_parameter.dispatch_depth = dispatch_depth;
            return trace_command;
        }
    };

    struct ComputeExecutionPlan
    {
        FrameDimensions frame_dimensions{};
        std::pair<unsigned, unsigned> dispatch_group_count{1u, 1u};

        static ComputeExecutionPlan FromFrameDimensions(
            const FrameDimensions& frame_dimensions,
            unsigned group_size_x = 8,
            unsigned group_size_y = 8)
        {
            return {
                .frame_dimensions = frame_dimensions,
                .dispatch_group_count = frame_dimensions.ComputeGroupCount2D(group_size_x, group_size_y)
            };
        }

        static ComputeExecutionPlan FromExtent(
            unsigned width,
            unsigned height,
            unsigned group_size_x = 8,
            unsigned group_size_y = 8)
        {
            return FromFrameDimensions(FrameDimensions::FromExtent(width, height), group_size_x, group_size_y);
        }

        static ComputeExecutionPlan FromResourceOperator(
            RendererInterface::ResourceOperator& resource_operator,
            unsigned group_size_x = 8,
            unsigned group_size_y = 8)
        {
            return FromFrameDimensions(
                FrameDimensions::FromResourceOperator(resource_operator),
                group_size_x,
                group_size_y);
        }

        RendererInterface::RenderExecuteCommand MakeDispatch(unsigned group_count_z = 1) const
        {
            RendererInterface::RenderExecuteCommand dispatch_command{};
            dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
            dispatch_command.parameter.dispatch_parameter.group_size_x = dispatch_group_count.first;
            dispatch_command.parameter.dispatch_parameter.group_size_y = dispatch_group_count.second;
            dispatch_command.parameter.dispatch_parameter.group_size_z = group_count_z;
            return dispatch_command;
        }

        void ApplyDispatch(
            RendererInterface::RenderGraph& graph,
            RendererInterface::RenderGraphNodeHandle node_handle,
            unsigned group_count_z = 1) const
        {
            graph.UpdateComputeDispatch(
                node_handle,
                dispatch_group_count.first,
                dispatch_group_count.second,
                group_count_z);
        }
    };

    struct GraphicsExecutionPlan
    {
        FrameDimensions viewport_dimensions{};

        static GraphicsExecutionPlan FromFrameDimensions(const FrameDimensions& viewport_dimensions)
        {
            return {
                .viewport_dimensions = viewport_dimensions
            };
        }

        static GraphicsExecutionPlan FromExtent(unsigned width, unsigned height)
        {
            return FromFrameDimensions(FrameDimensions::FromExtent(width, height));
        }

        static GraphicsExecutionPlan FromResourceOperator(RendererInterface::ResourceOperator& resource_operator)
        {
            return FromFrameDimensions(FrameDimensions::FromResourceOperator(resource_operator));
        }
    };

    struct RayTracingExecutionPlan
    {
        FrameDimensions dispatch_dimensions{};

        static RayTracingExecutionPlan FromFrameDimensions(const FrameDimensions& dispatch_dimensions)
        {
            return {
                .dispatch_dimensions = dispatch_dimensions
            };
        }

        static RayTracingExecutionPlan FromExtent(unsigned width, unsigned height)
        {
            return FromFrameDimensions(FrameDimensions::FromExtent(width, height));
        }

        static RayTracingExecutionPlan FromResourceOperator(RendererInterface::ResourceOperator& resource_operator)
        {
            return FromFrameDimensions(FrameDimensions::FromResourceOperator(resource_operator));
        }

        RendererInterface::RenderExecuteCommand MakeTraceRays(unsigned dispatch_depth = 1) const
        {
            return dispatch_dimensions.MakeTraceRays2D(dispatch_depth);
        }

        void ApplyDispatch(
            RendererInterface::RenderGraph& graph,
            RendererInterface::RenderGraphNodeHandle node_handle,
            unsigned dispatch_depth = 1) const
        {
            graph.UpdateRayTracingDispatch(
                node_handle,
                dispatch_dimensions.width,
                dispatch_dimensions.height,
                dispatch_depth);
        }
    };

    struct FrameSizedSetupState
    {
        bool valid{false};
        FrameDimensions frame_dimensions{};

        bool Matches(const FrameDimensions& candidate_dimensions) const
        {
            return valid &&
                   frame_dimensions.width == candidate_dimensions.width &&
                   frame_dimensions.height == candidate_dimensions.height;
        }

        void Update(const FrameDimensions& next_dimensions)
        {
            valid = true;
            frame_dimensions = next_dimensions;
        }

        void Reset()
        {
            valid = false;
        }
    };

    inline bool CreateRenderGraphNodeIfNeeded(
        RendererInterface::ResourceOperator& resource_operator,
        RendererInterface::RenderGraph& graph,
        RendererInterface::RenderGraphNodeHandle& inout_node_handle,
        const RendererInterface::RenderGraph::RenderPassSetupInfo& setup_info)
    {
        if (inout_node_handle != NULL_HANDLE)
        {
            return true;
        }

        inout_node_handle = graph.CreateRenderGraphNode(resource_operator, setup_info);
        return inout_node_handle != NULL_HANDLE;
    }

    inline bool SyncRenderGraphNode(
        RendererInterface::ResourceOperator& resource_operator,
        RendererInterface::RenderGraph& graph,
        RendererInterface::RenderGraphNodeHandle& inout_node_handle,
        const RendererInterface::RenderGraph::RenderPassSetupInfo& setup_info)
    {
        if (inout_node_handle == NULL_HANDLE)
        {
            return CreateRenderGraphNodeIfNeeded(resource_operator, graph, inout_node_handle, setup_info);
        }

        return graph.RebuildRenderGraphNode(resource_operator, inout_node_handle, setup_info);
    }

    inline bool CreateOrSyncRenderGraphNode(
        bool sync_existing,
        RendererInterface::ResourceOperator& resource_operator,
        RendererInterface::RenderGraph& graph,
        RendererInterface::RenderGraphNodeHandle& inout_node_handle,
        const RendererInterface::RenderGraph::RenderPassSetupInfo& setup_info)
    {
        if (sync_existing)
        {
            return SyncRenderGraphNode(resource_operator, graph, inout_node_handle, setup_info);
        }

        return CreateRenderGraphNodeIfNeeded(resource_operator, graph, inout_node_handle, setup_info);
    }

    inline bool RegisterRenderGraphNodeIfValid(
        RendererInterface::RenderGraph& graph,
        RendererInterface::RenderGraphNodeHandle node_handle)
    {
        if (node_handle == NULL_HANDLE)
        {
            return true;
        }

        return graph.RegisterRenderGraphNode(node_handle);
    }

    inline bool RegisterRenderGraphNodesIfValid(
        RendererInterface::RenderGraph& graph,
        std::initializer_list<RendererInterface::RenderGraphNodeHandle> node_handles)
    {
        for (const RendererInterface::RenderGraphNodeHandle node_handle : node_handles)
        {
            if (!RegisterRenderGraphNodeIfValid(graph, node_handle))
            {
                return false;
            }
        }

        return true;
    }

    struct RenderTargetAttachment
    {
        RendererInterface::RenderTargetHandle render_target_handle{NULL_HANDLE};
        RendererInterface::RenderTargetBindingDesc binding_desc{};
    };

    inline RendererInterface::RenderTargetBindingDesc MakeColorRenderTargetBinding(
        RendererInterface::PixelFormat format,
        bool need_clear)
    {
        RendererInterface::RenderTargetBindingDesc binding_desc{};
        binding_desc.format = format;
        binding_desc.usage = RendererInterface::RenderPassResourceUsage::COLOR;
        binding_desc.need_clear = need_clear;
        return binding_desc;
    }

    inline RenderTargetAttachment MakeRenderTargetAttachment(
        RendererInterface::RenderTargetHandle render_target_handle,
        const RendererInterface::RenderTargetBindingDesc& binding_desc)
    {
        return {
            .render_target_handle = render_target_handle,
            .binding_desc = binding_desc
        };
    }

    struct BufferBinding
    {
        std::string binding_name{};
        RendererInterface::BufferBindingDesc binding_desc{};
    };

    inline RendererInterface::RenderTargetBindingDesc MakeDepthRenderTargetBinding(
        RendererInterface::PixelFormat format,
        bool need_clear)
    {
        RendererInterface::RenderTargetBindingDesc binding_desc{};
        binding_desc.format = format;
        binding_desc.usage = RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL;
        binding_desc.need_clear = need_clear;
        return binding_desc;
    }

    inline RendererInterface::BufferBindingDesc MakeConstantBufferBinding(
        RendererInterface::BufferHandle buffer_handle)
    {
        RendererInterface::BufferBindingDesc binding_desc{};
        binding_desc.binding_type = RendererInterface::BufferBindingDesc::CBV;
        binding_desc.buffer_handle = buffer_handle;
        binding_desc.is_structured_buffer = false;
        return binding_desc;
    }

    inline RendererInterface::BufferBindingDesc MakeStructuredBufferBinding(
        RendererInterface::BufferHandle buffer_handle,
        RendererInterface::BufferBindingDesc::BufferBindingType binding_type,
        unsigned stride,
        unsigned count)
    {
        RendererInterface::BufferBindingDesc binding_desc{};
        binding_desc.binding_type = binding_type;
        binding_desc.buffer_handle = buffer_handle;
        binding_desc.stride = stride;
        binding_desc.count = count;
        binding_desc.is_structured_buffer = true;
        return binding_desc;
    }

    inline BufferBinding MakeBufferBinding(
        const std::string& binding_name,
        const RendererInterface::BufferBindingDesc& binding_desc)
    {
        return {
            .binding_name = binding_name,
            .binding_desc = binding_desc
        };
    }

    inline RendererInterface::RenderExecuteCommand MakeDispatch2D(
        unsigned width,
        unsigned height,
        unsigned group_size_x = 8,
        unsigned group_size_y = 8,
        unsigned group_size_z = 1)
    {
        return FrameDimensions::FromExtent(width, height).MakeDispatch2D(
            group_size_x,
            group_size_y,
            group_size_z);
    }

    inline RendererInterface::RenderExecuteCommand MakeTraceRays2D(
        unsigned width,
        unsigned height,
        unsigned dispatch_depth = 1)
    {
        return FrameDimensions::FromExtent(width, height).MakeTraceRays2D(dispatch_depth);
    }

    inline std::pair<unsigned, unsigned> MakeDispatchGroupCount2D(
        unsigned width,
        unsigned height,
        unsigned group_size_x = 8,
        unsigned group_size_y = 8)
    {
        return FrameDimensions::FromExtent(width, height).ComputeGroupCount2D(group_size_x, group_size_y);
    }

    struct SampledRenderTargetBinding
    {
        std::string binding_name{};
        std::vector<RendererInterface::RenderTargetHandle> render_target_handles{};
        RendererInterface::RenderTargetTextureBindingDesc::TextureBindingType binding_type{
            RendererInterface::RenderTargetTextureBindingDesc::SRV
        };
    };

    inline SampledRenderTargetBinding MakeSampledRenderTargetBinding(
        const std::string& binding_name,
        RendererInterface::RenderTargetHandle render_target_handle,
        RendererInterface::RenderTargetTextureBindingDesc::TextureBindingType binding_type)
    {
        return {
            .binding_name = binding_name,
            .render_target_handles = {render_target_handle},
            .binding_type = binding_type
        };
    }

    inline SampledRenderTargetBinding MakeSampledRenderTargetBinding(
        const std::string& binding_name,
        std::vector<RendererInterface::RenderTargetHandle> render_target_handles,
        RendererInterface::RenderTargetTextureBindingDesc::TextureBindingType binding_type)
    {
        return {
            .binding_name = binding_name,
            .render_target_handles = std::move(render_target_handles),
            .binding_type = binding_type
        };
    }

    class PassBuilder
    {
    public:
        static PassBuilder Graphics(const std::string& debug_group, const std::string& debug_name)
        {
            return PassBuilder(RendererInterface::RenderPassType::GRAPHICS, debug_group, debug_name);
        }

        static PassBuilder Compute(const std::string& debug_group, const std::string& debug_name)
        {
            return PassBuilder(RendererInterface::RenderPassType::COMPUTE, debug_group, debug_name);
        }

        static PassBuilder RayTracing(const std::string& debug_group, const std::string& debug_name)
        {
            return PassBuilder(RendererInterface::RenderPassType::RAY_TRACING, debug_group, debug_name);
        }

        PassBuilder& SetRenderState(const RendererInterface::RenderStateDesc& render_state)
        {
            m_setup_info.render_state = render_state;
            return *this;
        }

        PassBuilder& SetViewport(int width, int height)
        {
            m_setup_info.viewport_width = width;
            m_setup_info.viewport_height = height;
            return *this;
        }

        PassBuilder& SetViewport(const FrameDimensions& viewport_dimensions)
        {
            return SetViewport(
                static_cast<int>(viewport_dimensions.width),
                static_cast<int>(viewport_dimensions.height));
        }

        PassBuilder& SetViewport(const GraphicsExecutionPlan& execution_plan)
        {
            return SetViewport(execution_plan.viewport_dimensions);
        }

        PassBuilder& AddModule(const std::shared_ptr<RendererInterface::RendererModuleBase>& module)
        {
            if (module)
            {
                m_setup_info.modules.push_back(module);
            }
            return *this;
        }

        PassBuilder& AddModules(
            std::initializer_list<std::shared_ptr<RendererInterface::RendererModuleBase>> modules)
        {
            for (const auto& module : modules)
            {
                AddModule(module);
            }
            return *this;
        }

        PassBuilder& AddShader(
            RendererInterface::ShaderType shader_type,
            const std::string& entry_function,
            const std::string& shader_file)
        {
            m_setup_info.shader_setup_infos.push_back({
                .shader_type = shader_type,
                .entry_function = entry_function,
                .shader_file = shader_file
            });
            return *this;
        }

        PassBuilder& AddRenderTarget(
            RendererInterface::RenderTargetHandle render_target_handle,
            const RendererInterface::RenderTargetBindingDesc& binding_desc)
        {
            m_setup_info.render_targets[render_target_handle] = binding_desc;
            return *this;
        }

        PassBuilder& AddRenderTargets(std::initializer_list<RenderTargetAttachment> attachments)
        {
            for (const auto& attachment : attachments)
            {
                AddRenderTarget(attachment.render_target_handle, attachment.binding_desc);
            }
            return *this;
        }

        PassBuilder& AddSampledRenderTarget(
            const std::string& binding_name,
            RendererInterface::RenderTargetHandle render_target_handle,
            RendererInterface::RenderTargetTextureBindingDesc::TextureBindingType binding_type)
        {
            return AddSampledRenderTargets(
                binding_name,
                std::vector<RendererInterface::RenderTargetHandle>{render_target_handle},
                binding_type);
        }

        PassBuilder& AddSampledRenderTargets(
            const std::string& binding_name,
            const std::vector<RendererInterface::RenderTargetHandle>& render_target_handles,
            RendererInterface::RenderTargetTextureBindingDesc::TextureBindingType binding_type)
        {
            RendererInterface::RenderTargetTextureBindingDesc binding_desc{};
            binding_desc.name = binding_name;
            binding_desc.render_target_texture = render_target_handles;
            binding_desc.type = binding_type;
            m_setup_info.sampled_render_targets.push_back(std::move(binding_desc));
            return *this;
        }

        PassBuilder& AddSampledRenderTargetBinding(const SampledRenderTargetBinding& binding)
        {
            return AddSampledRenderTargets(
                binding.binding_name,
                binding.render_target_handles,
                binding.binding_type);
        }

        PassBuilder& AddSampledRenderTargetBindings(
            std::initializer_list<SampledRenderTargetBinding> bindings)
        {
            for (const auto& binding : bindings)
            {
                AddSampledRenderTargetBinding(binding);
            }
            return *this;
        }

        PassBuilder& AddBuffer(
            const std::string& binding_name,
            const RendererInterface::BufferBindingDesc& binding_desc)
        {
            m_setup_info.buffer_resources[binding_name] = binding_desc;
            return *this;
        }

        PassBuilder& AddBufferBinding(const BufferBinding& binding)
        {
            return AddBuffer(binding.binding_name, binding.binding_desc);
        }

        PassBuilder& AddBuffers(std::initializer_list<BufferBinding> bindings)
        {
            for (const auto& binding : bindings)
            {
                AddBufferBinding(binding);
            }
            return *this;
        }

        PassBuilder& AddTexture(
            const std::string& binding_name,
            const RendererInterface::TextureBindingDesc& binding_desc)
        {
            m_setup_info.texture_resources[binding_name] = binding_desc;
            return *this;
        }

        PassBuilder& ExcludeBufferBinding(const std::string& binding_name)
        {
            m_setup_info.excluded_buffer_bindings.insert(binding_name);
            return *this;
        }

        PassBuilder& ExcludeTextureBinding(const std::string& binding_name)
        {
            m_setup_info.excluded_texture_bindings.insert(binding_name);
            return *this;
        }

        PassBuilder& ExcludeRenderTargetTextureBinding(const std::string& binding_name)
        {
            m_setup_info.excluded_render_target_texture_bindings.insert(binding_name);
            return *this;
        }

        PassBuilder& AddDependency(RendererInterface::RenderGraphNodeHandle dependency_node)
        {
            if (dependency_node != NULL_HANDLE)
            {
                m_setup_info.dependency_render_graph_nodes.push_back(dependency_node);
            }
            return *this;
        }

        PassBuilder& AddDependencies(
            std::initializer_list<RendererInterface::RenderGraphNodeHandle> dependency_nodes)
        {
            for (const auto dependency_node : dependency_nodes)
            {
                AddDependency(dependency_node);
            }
            return *this;
        }

        PassBuilder& AddDependencies(
            const std::vector<RendererInterface::RenderGraphNodeHandle>& dependency_nodes)
        {
            for (const auto dependency_node : dependency_nodes)
            {
                AddDependency(dependency_node);
            }
            return *this;
        }

        PassBuilder& SetExecuteCommand(const RendererInterface::RenderExecuteCommand& execute_command)
        {
            m_setup_info.execute_command = execute_command;
            return *this;
        }

        PassBuilder& AddExecuteCommand(const RendererInterface::RenderExecuteCommand& execute_command)
        {
            m_setup_info.execute_commands.push_back(execute_command);
            return *this;
        }

        PassBuilder& SetDispatch2D(
            unsigned width,
            unsigned height,
            unsigned group_size_x = 8,
            unsigned group_size_y = 8,
            unsigned group_size_z = 1)
        {
            return SetExecuteCommand(MakeDispatch2D(width, height, group_size_x, group_size_y, group_size_z));
        }

        PassBuilder& SetDispatch(
            const ComputeExecutionPlan& execution_plan,
            unsigned group_count_z = 1)
        {
            return SetExecuteCommand(execution_plan.MakeDispatch(group_count_z));
        }

        PassBuilder& SetTraceRays(
            unsigned width,
            unsigned height,
            unsigned dispatch_depth = 1)
        {
            return SetExecuteCommand(MakeTraceRays2D(width, height, dispatch_depth));
        }

        PassBuilder& SetTraceRays(
            const FrameDimensions& dispatch_dimensions,
            unsigned dispatch_depth = 1)
        {
            return SetExecuteCommand(dispatch_dimensions.MakeTraceRays2D(dispatch_depth));
        }

        PassBuilder& SetTraceRays(
            const RayTracingExecutionPlan& execution_plan,
            unsigned dispatch_depth = 1)
        {
            return SetExecuteCommand(execution_plan.MakeTraceRays(dispatch_depth));
        }

        PassBuilder& SetRayTracingConfig(const RendererInterface::RayTracingConfig& config)
        {
            EnsureRayTracingDesc().config = config;
            return *this;
        }

        PassBuilder& SetRayTracingExportFunctionNames(const std::vector<std::string>& export_function_names)
        {
            EnsureRayTracingDesc().export_function_names = export_function_names;
            return *this;
        }

        PassBuilder& AddRayTracingHitGroup(const RendererInterface::RayTracingHitGroupDesc& hit_group_desc)
        {
            EnsureRayTracingDesc().hit_group_descs.push_back(hit_group_desc);
            return *this;
        }

        PassBuilder& SetRayTracingShaderTable(const std::shared_ptr<IRHIShaderTable>& shader_table)
        {
            EnsureRayTracingDesc().shader_table = shader_table;
            return *this;
        }

        PassBuilder& AddRayTracingAccelerationStructure(
            const std::string& binding_name,
            const std::shared_ptr<IRHIRayTracingAS>& acceleration_structure)
        {
            EnsureRayTracingDesc().acceleration_structure_bindings.push_back({
                .binding_name = binding_name,
                .acceleration_structure = acceleration_structure
            });
            return *this;
        }

        RendererInterface::RenderGraph::RenderPassSetupInfo Build()
        {
            return std::move(m_setup_info);
        }

    private:
        PassBuilder(
            RendererInterface::RenderPassType render_pass_type,
            const std::string& debug_group,
            const std::string& debug_name)
        {
            m_setup_info.render_pass_type = render_pass_type;
            m_setup_info.debug_group = debug_group;
            m_setup_info.debug_name = debug_name;
        }

        RendererInterface::RayTracingPassDesc& EnsureRayTracingDesc()
        {
            if (!m_setup_info.ray_tracing_desc.has_value())
            {
                m_setup_info.ray_tracing_desc = RendererInterface::RayTracingPassDesc{};
            }
            return m_setup_info.ray_tracing_desc.value();
        }

        RendererInterface::RenderGraph::RenderPassSetupInfo m_setup_info{};
    };

    inline PassBuilder MakeComputePostFxPassBuilder(
        const std::string& debug_group,
        const std::string& debug_name,
        const std::string& entry_function,
        const std::string& shader_file,
        RendererInterface::RenderTargetHandle input_rt,
        RendererInterface::RenderTargetHandle output_rt,
        unsigned dispatch_width,
        unsigned dispatch_height,
        RendererInterface::RenderGraphNodeHandle dependency_node = NULL_HANDLE,
        const std::string& input_binding_name = "InputTex",
        const std::string& output_binding_name = "OutputTex")
    {
        auto builder = PassBuilder::Compute(debug_group, debug_name);
        builder
            .AddShader(RendererInterface::COMPUTE_SHADER, entry_function, shader_file)
            .AddDependency(dependency_node)
            .AddSampledRenderTarget(
                input_binding_name,
                input_rt,
                RendererInterface::RenderTargetTextureBindingDesc::SRV)
            .AddSampledRenderTarget(
                output_binding_name,
                output_rt,
                RendererInterface::RenderTargetTextureBindingDesc::UAV)
            .SetDispatch2D(dispatch_width, dispatch_height);
        return builder;
    }
}

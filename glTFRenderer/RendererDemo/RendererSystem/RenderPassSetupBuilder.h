#pragma once

#include "RendererInterface.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RenderFeature
{
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

    inline RendererInterface::RenderExecuteCommand MakeDispatch2D(
        unsigned width,
        unsigned height,
        unsigned group_size_x = 8,
        unsigned group_size_y = 8,
        unsigned group_size_z = 1)
    {
        RendererInterface::RenderExecuteCommand dispatch_command{};
        dispatch_command.type = RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND;
        dispatch_command.parameter.dispatch_parameter.group_size_x =
            (width + group_size_x - 1u) / group_size_x;
        dispatch_command.parameter.dispatch_parameter.group_size_y =
            (height + group_size_y - 1u) / group_size_y;
        dispatch_command.parameter.dispatch_parameter.group_size_z = group_size_z;
        return dispatch_command;
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

        PassBuilder& AddBuffer(
            const std::string& binding_name,
            const RendererInterface::BufferBindingDesc& binding_desc)
        {
            m_setup_info.buffer_resources[binding_name] = binding_desc;
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

        RendererInterface::RenderGraph::RenderPassSetupInfo m_setup_info{};
    };
}

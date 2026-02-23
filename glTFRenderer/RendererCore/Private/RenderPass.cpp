#include "RenderPass.h"

#include "InternalResourceHandleTable.h"
#include "ResourceManager.h"
#include "RHIResourceFactoryImpl.hpp"
#include "RHIInterface/IRHIPipelineStateObject.h"

namespace
{
    RHICullMode ConvertToRHICullMode(RendererInterface::CullMode mode)
    {
        switch (mode)
        {
        case RendererInterface::CullMode::NONE:
            return RHICullMode::NONE;
        case RendererInterface::CullMode::CW:
            return RHICullMode::CW;
        case RendererInterface::CullMode::CCW:
            return RHICullMode::CCW;
        }

        GLTF_CHECK(false);
        return RHICullMode::NONE;
    }

    RHIDepthStencilMode ConvertToRHIDepthStencilMode(RendererInterface::DepthStencilMode mode)
    {
        switch (mode)
        {
        case RendererInterface::DepthStencilMode::DEPTH_READ:
            return RHIDepthStencilMode::DEPTH_READ;
        case RendererInterface::DepthStencilMode::DEPTH_WRITE:
            return RHIDepthStencilMode::DEPTH_WRITE;
        case RendererInterface::DepthStencilMode::DEPTH_DONT_CARE:
            return RHIDepthStencilMode::DEPTH_DONT_CARE;
        }

        GLTF_CHECK(false);
        return RHIDepthStencilMode::DEPTH_DONT_CARE;
    }

    const char* ToExecuteCommandTypeName(RendererInterface::ExecuteCommandType type)
    {
        using namespace RendererInterface;
        switch (type)
        {
        case ExecuteCommandType::DRAW_VERTEX_COMMAND:
            return "DRAW_VERTEX_COMMAND";
        case ExecuteCommandType::DRAW_VERTEX_INSTANCING_COMMAND:
            return "DRAW_VERTEX_INSTANCING_COMMAND";
        case ExecuteCommandType::DRAW_INDEXED_COMMAND:
            return "DRAW_INDEXED_COMMAND";
        case ExecuteCommandType::DRAW_INDEXED_INSTANCING_COMMAND:
            return "DRAW_INDEXED_INSTANCING_COMMAND";
        case ExecuteCommandType::COMPUTE_DISPATCH_COMMAND:
            return "COMPUTE_DISPATCH_COMMAND";
        case ExecuteCommandType::RAY_TRACING_COMMAND:
            return "RAY_TRACING_COMMAND";
        }
        return "UNKNOWN_COMMAND";
    }

    const char* ToRootParameterTypeName(RHIRootParameterType type)
    {
        switch (type)
        {
        case RHIRootParameterType::Constant:
            return "Constant";
        case RHIRootParameterType::CBV:
            return "CBV";
        case RHIRootParameterType::SRV:
            return "SRV";
        case RHIRootParameterType::UAV:
            return "UAV";
        case RHIRootParameterType::AccelerationStructure:
            return "AccelerationStructure";
        case RHIRootParameterType::DescriptorTable:
            return "DescriptorTable";
        case RHIRootParameterType::Sampler:
            return "Sampler";
        case RHIRootParameterType::Unknown:
            return "Unknown";
        }
        return "Unknown";
    }

    const char* ToDescriptorRangeTypeName(RHIDescriptorRangeType type)
    {
        switch (type)
        {
        case RHIDescriptorRangeType::CBV:
            return "CBV";
        case RHIDescriptorRangeType::SRV:
            return "SRV";
        case RHIDescriptorRangeType::UAV:
            return "UAV";
        case RHIDescriptorRangeType::Unknown:
            return "Unknown";
        }
        return "Unknown";
    }

    bool IsEquivalentShaderParameter(const ShaderMetaDataParameter& lhs, const ShaderMetaDataParameter& rhs)
    {
        if (lhs.space != rhs.space || lhs.register_index != rhs.register_index)
        {
            return false;
        }

        const auto& lhs_info = lhs.parameter_info;
        const auto& rhs_info = rhs.parameter_info;
        if (lhs_info.type != rhs_info.type ||
            lhs_info.register_count != rhs_info.register_count ||
            lhs_info.is_buffer != rhs_info.is_buffer)
        {
            return false;
        }

        switch (lhs_info.type)
        {
        case RHIRootParameterType::Constant:
            return lhs_info.constant_parameter_info.constant_value == rhs_info.constant_parameter_info.constant_value;
        case RHIRootParameterType::DescriptorTable:
            return lhs_info.table_parameter_info.table_type == rhs_info.table_parameter_info.table_type &&
                   lhs_info.table_parameter_info.is_bindless == rhs_info.table_parameter_info.is_bindless;
        case RHIRootParameterType::Sampler:
            return lhs_info.sampler_parameter_info.address_mode == rhs_info.sampler_parameter_info.address_mode &&
                   lhs_info.sampler_parameter_info.filter_mode == rhs_info.sampler_parameter_info.filter_mode;
        default:
            return true;
        }
    }

    void LogShaderParameterConflict(const std::string& parameter_name,
                                    const ShaderMetaDataParameter& existing,
                                    const ShaderMetaDataParameter& incoming)
    {
        const auto& existing_info = existing.parameter_info;
        const auto& incoming_info = incoming.parameter_info;

        LOG_FORMAT_FLUSH("[RenderPass][Init][Error] Shader parameter '%s' has conflicting reflection metadata.\n",
                         parameter_name.c_str());
        LOG_FORMAT_FLUSH("[RenderPass][Init][Error]   Existing: type=%s, space=%u, reg=%u, count=%u, is_buffer=%d, table_type=%s, bindless=%d\n",
                         ToRootParameterTypeName(existing_info.type),
                         existing.space,
                         existing.register_index,
                         existing_info.register_count,
                         existing_info.is_buffer ? 1 : 0,
                         ToDescriptorRangeTypeName(existing_info.type == RHIRootParameterType::DescriptorTable
                                                       ? existing_info.table_parameter_info.table_type
                                                       : RHIDescriptorRangeType::Unknown),
                         existing_info.type == RHIRootParameterType::DescriptorTable && existing_info.table_parameter_info.is_bindless ? 1 : 0);
        LOG_FORMAT_FLUSH("[RenderPass][Init][Error]   Incoming: type=%s, space=%u, reg=%u, count=%u, is_buffer=%d, table_type=%s, bindless=%d\n",
                         ToRootParameterTypeName(incoming_info.type),
                         incoming.space,
                         incoming.register_index,
                         incoming_info.register_count,
                         incoming_info.is_buffer ? 1 : 0,
                         ToDescriptorRangeTypeName(incoming_info.type == RHIRootParameterType::DescriptorTable
                                                       ? incoming_info.table_parameter_info.table_type
                                                       : RHIDescriptorRangeType::Unknown),
                         incoming_info.type == RHIRootParameterType::DescriptorTable && incoming_info.table_parameter_info.is_bindless ? 1 : 0);
    }

    bool IsCompatibleBufferBindingType(const RootSignatureAllocation& allocation, RendererInterface::BufferBindingDesc::BufferBindingType binding_type)
    {
        switch (binding_type)
        {
        case RendererInterface::BufferBindingDesc::CBV:
            return allocation.type == RHIRootParameterType::CBV || allocation.type == RHIRootParameterType::DescriptorTable;
        case RendererInterface::BufferBindingDesc::SRV:
            return allocation.type == RHIRootParameterType::SRV || allocation.type == RHIRootParameterType::DescriptorTable;
        case RendererInterface::BufferBindingDesc::UAV:
            return allocation.type == RHIRootParameterType::UAV || allocation.type == RHIRootParameterType::DescriptorTable;
        }
        return false;
    }

    bool IsCompatibleTextureBindingType(const RootSignatureAllocation& allocation, bool is_srv)
    {
        if (is_srv)
        {
            return allocation.type == RHIRootParameterType::SRV || allocation.type == RHIRootParameterType::DescriptorTable;
        }
        return allocation.type == RHIRootParameterType::UAV || allocation.type == RHIRootParameterType::DescriptorTable;
    }
}

RenderPass::RenderPass(RendererInterface::RenderPassDesc desc)
    : m_desc(std::move(desc))
{
    m_viewport_width = m_desc.viewport_width;
    m_viewport_height = m_desc.viewport_height;
}

bool RenderPass::InitRenderPass(ResourceManager& resource_manager)
{
    m_init_success = false;

    switch (m_desc.type) {
    case RendererInterface::RenderPassType::GRAPHICS:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIGraphicsPipelineStateObject>();
        break;
    case RendererInterface::RenderPassType::COMPUTE:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIComputePipelineStateObject>();
        break;
    case RendererInterface::RenderPassType::RAY_TRACING:
        m_pipeline_state_object = RHIResourceFactory::CreateRHIResource<IRHIRayTracingPipelineStateObject>();
        break;
    }

    // Build root signature from shader reflection
    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> shaders;
    std::map<std::string, ShaderMetaDataParameter> unique_parameters;
    for (const auto& shader_pair : m_desc.shaders)
    {
        auto shader = RendererInterface::InternalResourceHandleTable::Instance().GetShader(shader_pair.second);
        auto& shader_meta_data = shader->GetMetaData();
        for (const auto& root_parameter_info : shader_meta_data.root_parameter_infos)
        {
            const auto& parameter_name = root_parameter_info.parameter_info.parameter_name;
            const auto existing_parameter_it = unique_parameters.find(parameter_name);
            if (existing_parameter_it != unique_parameters.end())
            {
                if (!IsEquivalentShaderParameter(existing_parameter_it->second, root_parameter_info))
                {
                    LogShaderParameterConflict(parameter_name, existing_parameter_it->second, root_parameter_info);
                    return false;
                }
                continue;
            }

            RootSignatureAllocation allocation;
            if (!m_root_signature_helper.AddRootParameterWithRegisterCount2(
                root_parameter_info.parameter_info,
                root_parameter_info.space,
                root_parameter_info.register_index,
                allocation))
            {
                LOG_FORMAT_FLUSH("[RenderPass][Init][Error] Failed to add root parameter '%s' (space=%u, reg=%u).\n",
                                 parameter_name.c_str(),
                                 root_parameter_info.space,
                                 root_parameter_info.register_index);
                return false;
            }

            unique_parameters.insert({parameter_name, root_parameter_info});
            m_shader_parameter_mapping.insert({parameter_name, allocation});
        }

        shaders[shader->GetType()] = shader;
    }
    
    // Init root signature
    m_root_signature_helper.BuildRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager());
    auto* m_root_signature = &m_root_signature_helper.GetRootSignature(); 
    
    m_root_signature->InitRootSignature(resource_manager.GetDevice(), resource_manager.GetMemoryManager().GetDescriptorManager());

    if (m_pipeline_state_object->GetPSOType() == RHIPipelineType::Graphics)
    {
        auto& graphics_pipeline_state_object = dynamic_cast<IRHIGraphicsPipelineStateObject&>(*m_pipeline_state_object);
        std::vector<RHIDataFormat> render_target_formats;
        render_target_formats.reserve(m_desc.render_target_bindings.size());
        for (const auto& render_target : m_desc.render_target_bindings)
        {
            render_target_formats.push_back(RendererInterfaceRHIConverter::ConvertToRHIFormat(render_target.format));
        }
        
        graphics_pipeline_state_object.BindRenderTargetFormats(render_target_formats);
    }

    m_pipeline_state_object->SetCullMode(ConvertToRHICullMode(m_desc.render_state.cull_mode));
    m_pipeline_state_object->SetDepthStencilState(ConvertToRHIDepthStencilMode(m_desc.render_state.depth_stencil_mode));
    
    if (!m_pipeline_state_object->InitPipelineStateObject(resource_manager.GetDevice(),
            *m_root_signature,
            resource_manager.GetSwapChain(),
            shaders
        ))
    {
        LOG_FORMAT_FLUSH("[RenderPass][Init][Error] InitPipelineStateObject failed. pass_type=%d shader_count=%zu\n",
                         static_cast<int>(m_desc.type),
                         shaders.size());
        return false;
    }

    m_descriptor_updater = RHIResourceFactory::CreateRHIResource<IRHIDescriptorUpdater>();
    m_init_success = true;

    return true;
}

IRHIPipelineStateObject& RenderPass::GetPipelineStateObject()
{
    return *m_pipeline_state_object;
}

IRHIRootSignature& RenderPass::GetRootSignature()
{
    return m_root_signature_helper.GetRootSignature();
}

RendererInterface::RenderPassType RenderPass::GetRenderPassType() const
{
    return m_desc.type;
}

RendererInterface::PrimitiveTopology RenderPass::GetPrimitiveTopology() const
{
    return m_desc.render_state.primitive_topology;
}

const RootSignatureAllocation& RenderPass::GetRootSignatureAllocation(const std::string& name) const
{
    GLTF_CHECK(m_shader_parameter_mapping.contains(name));

    return m_shader_parameter_mapping.at(name);
}

const RootSignatureAllocation* RenderPass::FindRootSignatureAllocation(const std::string& name) const
{
    if (!m_shader_parameter_mapping.contains(name))
    {
        return nullptr;
    }
    return &m_shader_parameter_mapping.at(name);
}

bool RenderPass::HasRootSignatureAllocation(const std::string& name) const
{
    return m_shader_parameter_mapping.contains(name);
}

RenderPass::DrawValidationResult RenderPass::ValidateDrawDesc(const RendererInterface::RenderPassDrawDesc& draw_desc) const
{
    DrawValidationResult result{};

    auto push_error = [&](const std::string& message)
    {
        result.valid = false;
        result.errors.push_back(message);
    };
    auto push_warning = [&](const std::string& message)
    {
        result.warnings.push_back(message);
    };

    switch (m_desc.type)
    {
    case RendererInterface::RenderPassType::GRAPHICS:
        for (const auto& command : draw_desc.execute_commands)
        {
            if (command.type == RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND)
            {
                push_error(std::string("Graphics pass contains unsupported command type: ") + ToExecuteCommandTypeName(command.type));
            }
        }
        break;
    case RendererInterface::RenderPassType::COMPUTE:
        for (const auto& command : draw_desc.execute_commands)
        {
            if (command.type != RendererInterface::ExecuteCommandType::COMPUTE_DISPATCH_COMMAND)
            {
                push_error(std::string("Compute pass contains unsupported command type: ") + ToExecuteCommandTypeName(command.type));
            }
        }
        break;
    case RendererInterface::RenderPassType::RAY_TRACING:
        for (const auto& command : draw_desc.execute_commands)
        {
            if (command.type != RendererInterface::ExecuteCommandType::RAY_TRACING_COMMAND)
            {
                push_error(std::string("Ray tracing pass contains unsupported command type: ") + ToExecuteCommandTypeName(command.type));
            }
        }
        break;
    }

    if (m_desc.type == RendererInterface::RenderPassType::GRAPHICS)
    {
        unsigned expected_color_count = 0;
        unsigned expected_depth_count = 0;
        for (const auto& attachment : m_desc.render_target_bindings)
        {
            if (attachment.usage == RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL)
            {
                ++expected_depth_count;
            }
            else
            {
                ++expected_color_count;
            }
        }

        unsigned actual_color_count = 0;
        unsigned actual_depth_count = 0;
        for (const auto& render_target : draw_desc.render_target_resources)
        {
            if (!render_target.first.IsValid())
            {
                push_error("Graphics pass contains invalid render target handle.");
                continue;
            }

            if (render_target.second.usage == RendererInterface::RenderPassResourceUsage::DEPTH_STENCIL)
            {
                ++actual_depth_count;
            }
            else
            {
                ++actual_color_count;
            }
        }

        if (actual_color_count < expected_color_count)
        {
            push_error("Graphics pass is missing required color attachments for PSO.");
        }
        if (actual_depth_count < expected_depth_count)
        {
            push_error("Graphics pass is missing required depth/stencil attachment for PSO.");
        }
    }

    for (const auto& buffer : draw_desc.buffer_resources)
    {
        if (!buffer.second.buffer_handle.IsValid())
        {
            push_error("Buffer binding '" + buffer.first + "' has invalid handle.");
            continue;
        }

        const auto* allocation = FindRootSignatureAllocation(buffer.first);
        if (allocation == nullptr)
        {
            push_warning("Buffer binding '" + buffer.first + "' not found in root signature. Binding will be ignored.");
            continue;
        }

        if (!IsCompatibleBufferBindingType(*allocation, buffer.second.binding_type))
        {
            push_warning("Buffer binding '" + buffer.first + "' type mismatch with root signature type " + ToRootParameterTypeName(allocation->type) + ".");
        }
    }

    for (const auto& texture : draw_desc.texture_resources)
    {
        if (texture.second.textures.empty())
        {
            push_error("Texture binding '" + texture.first + "' has empty texture list.");
            continue;
        }

        for (const auto handle : texture.second.textures)
        {
            if (!handle.IsValid())
            {
                push_error("Texture binding '" + texture.first + "' has invalid texture handle.");
            }
        }

        const auto* allocation = FindRootSignatureAllocation(texture.first);
        if (allocation == nullptr)
        {
            push_warning("Texture binding '" + texture.first + "' not found in root signature. Binding will be ignored.");
            continue;
        }

        if (!IsCompatibleTextureBindingType(*allocation, texture.second.type == RendererInterface::TextureBindingDesc::SRV))
        {
            push_warning("Texture binding '" + texture.first + "' type mismatch with root signature type " + ToRootParameterTypeName(allocation->type) + ".");
        }
    }

    for (const auto& render_target_texture : draw_desc.render_target_texture_resources)
    {
        if (render_target_texture.second.render_target_texture.empty())
        {
            push_error("RenderTarget texture binding '" + render_target_texture.first + "' has empty render target list.");
            continue;
        }

        for (const auto handle : render_target_texture.second.render_target_texture)
        {
            if (!handle.IsValid())
            {
                push_error("RenderTarget texture binding '" + render_target_texture.first + "' has invalid render target handle.");
            }
        }

        const auto* allocation = FindRootSignatureAllocation(render_target_texture.first);
        if (allocation == nullptr)
        {
            push_warning("RenderTarget texture binding '" + render_target_texture.first + "' not found in root signature. Binding will be ignored.");
            continue;
        }

        if (!IsCompatibleTextureBindingType(*allocation, render_target_texture.second.type == RendererInterface::RenderTargetTextureBindingDesc::SRV))
        {
            push_warning("RenderTarget texture binding '" + render_target_texture.first + "' type mismatch with root signature type " + ToRootParameterTypeName(allocation->type) + ".");
        }
    }

    return result;
}

IRHIDescriptorUpdater& RenderPass::GetDescriptorUpdater()
{
    return *m_descriptor_updater;
}

std::pair<int, int> RenderPass::GetViewportSize() const
{
    return {m_viewport_width, m_viewport_height};
}

void RenderPass::SetViewportSize(int width, int height)
{
    m_viewport_width = width;
    m_viewport_height = height;
}

bool RenderPass::IsInitialized() const
{
    return m_init_success;
}

#pragma once
#include <map>
#include "IRHIDevice.h"
#include "IRHIRenderPass.h"
#include "IRHIRootSignature.h"
#include "IRHIResource.h"
#include "IRHISwapChain.h"
#include "ShaderUtil/IRHIShader.h"

class IRHIDescriptorAllocation;
class IRHIRenderTarget;

#define DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(x) static const char* g_inputLayoutName_##x = #x;

DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(POSITION)
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(NORMAL)
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(TANGENT)
DECLARE_INPUT_LAYOUT_SEMANTIC_NAME(TEXCOORD)

#define INPUT_LAYOUT_UNIQUE_PARAMETER(x) (g_inputLayoutName_##x)

class IRHIPipelineStateObject : public IRHIResource
{
public:
    DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(IRHIPipelineStateObject)
    
    IRHIPipelineStateObject(RHIPipelineType type);

    virtual bool InitPipelineStateObject(IRHIDevice& device, const IRHIRootSignature& root_signature, IRHISwapChain& swap_chain, const std::vector<RHIPipelineInputLayout>& input_layouts) = 0;
    
    bool BindShaderCode(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name);
    
    IRHIShader& GetBindShader(RHIShaderType type);
    
    void SetCullMode(RHICullMode mode);
    void SetDepthStencilState(RHIDepthStencilMode state);

    RHIPipelineType GetPSOType() const {return m_type; }
    RHICullMode GetCullMode() const;
    RHIShaderPreDefineMacros& GetShaderMacros();
    
protected:
    bool CompileShaders();
    
    RHIPipelineType m_type;
    RHIShaderPreDefineMacros m_shader_macros;
    RHICullMode m_cullMode;
    RHIDepthStencilMode m_depth_stencil_state;
    std::map<RHIShaderType, std::shared_ptr<IRHIShader>> m_shaders;
};

class IRHIGraphicsPipelineStateObject : public IRHIPipelineStateObject
{
public:
    IRHIGraphicsPipelineStateObject();

    virtual bool BindRenderTargetFormats(const std::vector<IRHIRenderTarget*>& render_targets) = 0;
    virtual bool BindRenderTargetFormats(const std::vector<IRHIDescriptorAllocation*>& render_targets) = 0;
};

class IRHIComputePipelineStateObject : public IRHIPipelineStateObject
{
public:
    IRHIComputePipelineStateObject();
};

class IRHIRayTracingPipelineStateObject : public IRHIPipelineStateObject
{
public:
    struct RHIRayTracingHitGroupDesc
    {
        std::string m_export_hit_group_name;
        std::string m_closest_hit_entry_name;
        std::string m_any_hit_entry_name;
        std::string m_intersection_entry_name;
    };

    struct RHIRayTracingLocalRSDesc
    {
        std::shared_ptr<IRHIRootSignature> m_root_signature;
        std::vector<std::string> export_names;
    };

    struct RHIRayTracingConfig
    {
        // Shader config
        unsigned payload_size;
        unsigned attribute_size;

        // Pipeline config
        unsigned max_recursion_count;
    };

    struct RayTracingShaderEntryFunctionNames
    {
        std::string raygen_shader_entry_name;
        std::string miss_shader_entry_name;
        std::string closest_hit_shader_entry_name;
        std::string any_hit_shader_entry_name;
        std::string intersection_shader_entry_name;

        std::vector<std::string> GetValidEntryFunctionNames() const
        {
            std::vector<std::string> result;
        
            if (!raygen_shader_entry_name.empty())
            {
                result.push_back(raygen_shader_entry_name);
            }

            if (!miss_shader_entry_name.empty())
            {
                result.push_back(miss_shader_entry_name);
            }

            if (!closest_hit_shader_entry_name.empty())
            {
                result.push_back(closest_hit_shader_entry_name);
            }

            if (!any_hit_shader_entry_name.empty())
            {
                result.push_back(any_hit_shader_entry_name);
            }
        
            return result;
        }
    };
    
    IRHIRayTracingPipelineStateObject();

    void AddHitGroupDesc(const RHIRayTracingHitGroupDesc& desc);
    void AddLocalRSDesc(const RHIRayTracingLocalRSDesc& desc);

    void SetExportFunctionNames(const std::vector<std::string>& export_names);
    void SetConfig(const RHIRayTracingConfig& config);
    
    const std::vector<RHIRayTracingHitGroupDesc>& GetHitGroupDescs() const {return m_hit_group_descs; }
    
protected:
    std::vector<RHIRayTracingHitGroupDesc> m_hit_group_descs;
    std::vector<RHIRayTracingLocalRSDesc> m_local_rs_descs;
    std::vector<std::string> m_export_function_names;
    
    RHIRayTracingConfig m_config;
};
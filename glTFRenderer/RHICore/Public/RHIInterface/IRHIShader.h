#pragma once
#include "RendererCommon.h"
#include "RHICommon.h"

enum class ShaderMetaDataDSType
{
    Constant,
    CBV,
    SRV,
    UAV,
    Sampler,
};

enum class ShaderMetaDataResourceType
{
    // Set by cpu, skip cbv buffer binding
    Constant,

    ConstantBuffer,
    
    StructuredBuffer,

    // Append only structured buffer
    AppendStructuredBuffer,

    // Consume only structured buffer
    ConsumeStructuredBuffer,
    
    Texture,

    AccelerationStructure,
    
};

struct ShaderMetaDataDSParameter
{
    
    ShaderMetaDataDSType descriptor_type;
    ShaderMetaDataResourceType resource_type;
    std::string name;
    unsigned binding_index;
    unsigned space_index;
};

struct ShaderMetaData
{
    //std::vector<ShaderMetaDataDSParameter> parameter_infos;
    std::vector<RootParameterInfo> root_parameter_infos;
};

class glTFShaderUtils
{
public:
    enum class ShaderFileType
    {
        SFT_HLSL,
        SFT_GLSL,
    };

    enum class ShaderType
    {
        ST_Vertex,
        ST_Fragment,
        ST_Compute,
        ST_Undefined,
    };
    
    struct ShaderCompileDesc
    {
        std::string file_path;
        std::string compile_target;
        std::string entry_function;
        RHIShaderPreDefineMacros shader_macros;
        bool spirv; // Vulkan binary format
        ShaderFileType file_type;
        ShaderType shader_type;
    };
    
    static bool IsShaderFileExist(const char* shaderFilePath);
    static bool LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent);
    static std::string GetShaderCompileTarget(RHIShaderType type);
    static ShaderType GetShaderType(RHIShaderType type);
    static bool CompileShader(const ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries);
};

class IRHIShader 
{
public:
    IRHIShader(RHIShaderType type = RHIShaderType::Unknown);
    virtual ~IRHIShader() = default;
    
    RHIShaderType GetType() const;
    const std::string& GetShaderContent() const;
    const std::string& GetMainEntry() const;
    void SetShaderCompilePreDefineMacros(const RHIShaderPreDefineMacros& macros);
    
    bool InitShader(const std::string& shader_file_path, RHIShaderType type, const std::string& entry_function_name);
    bool CompileShader();
    
    const std::vector<unsigned char>& GetShaderByteCode() const;
    
    ShaderMetaData& GetMetaData();
    
protected:
    std::string GetShaderTypeName() const;
    bool LoadShader(const std::string& shader_file_path);
    
    RHIShaderType m_type;
    std::string m_shader_file_path;
    
    std::string m_shader_entry_function_name;
    std::string m_shader_content;
    std::string m_shaderFilePath;

    RHIShaderPreDefineMacros m_macros;
    std::vector<unsigned char> m_shader_byte_code;

    ShaderMetaData m_shader_meta_data;
};
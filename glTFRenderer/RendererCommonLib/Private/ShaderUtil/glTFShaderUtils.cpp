#include "ShaderUtil/glTFShaderUtils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <fstream>
#include <sstream>

#include <d3dcompiler.h>
#include <dxcapi.h>
#include <atlbase.h>

#include "RendererCommon.h"

bool glTFShaderUtils::IsShaderFileExist(const char* shaderFilePath)
{
    const std::fstream shaderFileStream(shaderFilePath, std::ios::in);
    return shaderFileStream.good();
}

bool glTFShaderUtils::LoadShaderFile(const char* shaderFilePath, std::string& outShaderFileContent)
{
    std::ifstream shaderFileStream(shaderFilePath, std::ios::in);
    if (shaderFileStream.bad())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << shaderFileStream.rdbuf();

    outShaderFileContent = buffer.str();
	outShaderFileContent += '\0';
    
    return true;
}

std::string glTFShaderUtils::GetShaderCompileTarget(RHIShaderType type)
{
    switch (type) {
        case RHIShaderType::Vertex: return "vs_6_8";
        case RHIShaderType::Pixel: return "ps_6_8";
        case RHIShaderType::Compute: return "cs_6_8";
        
        // raytracing shader will be compile into lib
        case RHIShaderType::RayTracing: return "lib_6_8";
    }

    assert(false);
    return "UnknownTarget";
}

bool CompileShaderWithFXC(const glTFShaderUtils::ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries)
{
    std::string shader_content;
    glTFShaderUtils::LoadShaderFile(compile_desc.file_path.c_str(), shader_content);
    
    if (shader_content.empty() || compile_desc.entry_function.empty())
    {
        // Not init shader content!
        assert(false);
        return false;
    }
    
    assert(compile_desc.shader_macros.macroKey.size() == compile_desc.shader_macros.macroValue.size());
    std::vector<D3D_SHADER_MACRO> dxShaderMacros;
    if (!compile_desc.shader_macros.macroKey.empty())
    {
        for (size_t i = 0; i < compile_desc.shader_macros.macroKey.size(); i++)
        {
            const std::string& key = compile_desc.shader_macros.macroKey[i];
            const std::string& value = compile_desc.shader_macros.macroValue[i];
            
            dxShaderMacros.push_back({key.c_str(), value.c_str()});
            LOG_FORMAT_FLUSH("[DEBUG] Compile shader:%s with macro %s = %s\n", compile_desc.file_path.c_str(), key.c_str(), value.c_str())
        }
        dxShaderMacros.push_back({nullptr, nullptr});
    }
    
    ID3DBlob* shaderCompileResult = nullptr; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff = nullptr; // a buffer holding the error data if any
    
    D3DCompile(shader_content.data(),
            shader_content.length(),
            nullptr,
            dxShaderMacros.data(),
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            compile_desc.entry_function.c_str(),
            compile_desc.compile_target.c_str(), 
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &shaderCompileResult,
            &errorBuff);

    if (errorBuff)
    {
        LOG_FORMAT_FLUSH("[FATAL] Compile shader file %s failed\nresult: %s\n", compile_desc.file_path.c_str(), static_cast<const char*>(errorBuff->GetBufferPointer()));
        return false;
    }
    
    // store shader bytecode
    out_binaries.resize(shaderCompileResult->GetBufferSize());
    memcpy(out_binaries.data(), shaderCompileResult->GetBufferPointer(), shaderCompileResult->GetBufferSize());
    
    return true;
}

bool CompileShaderWithDXC(const glTFShaderUtils::ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries)
{
    CComPtr<IDxcUtils> pUtils;
    CComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

    //
    // Create default include handler. (You can create your own...)
    //
    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

    const auto file_path = to_wide_string(compile_desc.file_path);
    const auto target = to_wide_string(compile_desc.compile_target);

    TCHAR working_directory[MAX_PATH] = {0};
    GetCurrentDirectory(MAX_PATH, working_directory);
    
    std::vector<std::wstring> compile_argument_strings;
    compile_argument_strings.emplace_back(file_path.c_str());   // Optional shader source file name for error reporting and for PIX shader source view.
    compile_argument_strings.emplace_back(L"-T");
    compile_argument_strings.emplace_back(target.c_str());      // Target.
    compile_argument_strings.emplace_back(L"-Zi");              // Enable debug information.
    //compile_argument_strings.emplace_back(L"-Fo");
    //compile_argument_strings.emplace_back(L"myshader.bin");     // Optional. Stored in the pdb.
    compile_argument_strings.emplace_back(L"-I");
    compile_argument_strings.emplace_back(working_directory);

    if (compile_desc.spirv)
    {
        compile_argument_strings.emplace_back(L"-spirv");
    }
    else
    {
        compile_argument_strings.emplace_back(L"-Fd");
        compile_argument_strings.emplace_back(L"-Qstrip_reflect");  // Strip reflection into a separate blob.
        compile_argument_strings.emplace_back(L"-Qstrip_debug");  // Strip debug into a separate blob.
    }
    
#ifdef DEBUG
    compile_argument_strings.emplace_back(L"-Od");
#endif
    
    if (!compile_desc.entry_function.empty())
    {
        compile_argument_strings.emplace_back(L"-E");
        compile_argument_strings.emplace_back(to_wide_string(compile_desc.entry_function));
    }
    
    if (!compile_desc.shader_macros.macroKey.empty())
    {
        for (size_t i = 0; i < compile_desc.shader_macros.macroKey.size(); i++)
        {
            char define_string[256] = {'\0'};
            (void)snprintf(define_string, sizeof(define_string), "%s=%s", compile_desc.shader_macros.macroKey[i].c_str(), compile_desc.shader_macros.macroValue[i].c_str());
            LOG_FORMAT_FLUSH("[DEBUG] Compile shader file %s with macro %s\n", compile_desc.file_path.c_str(), define_string)
            
            compile_argument_strings.emplace_back(L"-D"); 
            compile_argument_strings.emplace_back(to_wide_string(std::string(define_string)));
        }
    }

    std::vector<LPCWSTR> compile_argument_raws;
    compile_argument_raws.reserve(compile_argument_strings.size());
    for (const auto& argument : compile_argument_strings )
    {
        compile_argument_raws.push_back(argument.c_str());
    }
    
    // Open source file.  
    CComPtr<IDxcBlobEncoding> pSource = nullptr;
    THROW_IF_FAILED(pUtils->LoadFile(file_path.c_str(), nullptr, &pSource));
    GLTF_CHECK(pSource);
    
    DxcBuffer Source;
    Source.Ptr = pSource->GetBufferPointer();
    Source.Size = pSource->GetBufferSize();
    Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    // Compile it with specified arguments.
    CComPtr<IDxcResult> pResults;
    THROW_IF_FAILED(pCompiler->Compile(
        &Source,                // Source buffer.
        compile_argument_raws.data(),                // Array of pointers to arguments.
        compile_argument_raws.size(),      // Number of arguments.
        pIncludeHandler,        // User-provided interface to handle #include directives (optional).
        IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.) 
    ))

    // Print errors if present.
    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    // Note that d3dcompiler would return null if no errors or warnings are present.  
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());
        LOG_FLUSH()
    }

    // Quit if the compilation failed.
    HRESULT hrStatus;
    pResults->GetStatus(&hrStatus);
    if (FAILED(hrStatus))
    {
        wprintf(L"Compilation Failed\n");
        return false;
    }

    static auto save_dxc_compile_output_to_disk = []( IDxcBlob* dxc_data, const std::string& output_file_path)
    {
        auto* data = dxc_data->GetBufferPointer();
        const auto data_size = dxc_data->GetBufferSize();

        std::ofstream output_file_stream(output_file_path, std::ios::out | std::ios::binary);
        GLTF_CHECK (output_file_stream.is_open());

        output_file_stream.write(static_cast<const char*>(data), data_size);
        output_file_stream.close();

        LOG_FORMAT("[DEBUG] Output shader compile output to file: %s\n", output_file_path.c_str());
    };
    
    // Save shader binary.
    CComPtr<IDxcBlob> pShader = nullptr;
    CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
    THROW_IF_FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pShaderName))
    
    out_binaries.resize(pShader->GetBufferSize());
    memcpy(out_binaries.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());

    std::string output_shader_hash = compile_desc.file_path;
    
    if (pResults->HasOutput(DXC_OUT_SHADER_HASH))
    {
        CComPtr<IDxcBlob> shader_hash = nullptr;
        THROW_IF_FAILED(pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&shader_hash), nullptr))
        if (shader_hash && shader_hash->GetBufferSize())
        {
            auto* pHashBuf = static_cast<DxcShaderHash*>(shader_hash->GetBufferPointer());
            char hash_string[32] = {'\0'};
            for(size_t i = 0; i < _countof(pHashBuf->HashDigest); ++i) {
                (void)snprintf(hash_string + i, 16, "%X", pHashBuf->HashDigest[i]);
            }

            const auto last_slash = std::min(compile_desc.file_path.find_last_of('\\'), compile_desc.file_path.find_last_of('/'));
            output_shader_hash = std::string(compile_desc.file_path.c_str(), last_slash + 1) + std::string(hash_string);
        }
    }
    
    
    save_dxc_compile_output_to_disk( pShader, output_shader_hash + ".bin");
    if (pResults->HasOutput(DXC_OUT_PDB))
    {
        // Save shader pdb file.
        CComPtr<IDxcBlob> pShaderPDB = nullptr;
        CComPtr<IDxcBlobUtf16> pShaderPDBName = nullptr;
        if (SUCCEEDED(pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pShaderPDB), &pShaderPDBName)))
        {
            save_dxc_compile_output_to_disk(pShaderPDB, output_shader_hash + ".pdb");
        }    
    }
    
    return true;
}

struct SpirvHelper
{
	static bool GLSLtoSPVExternal(const glTFShaderUtils::ShaderCompileDesc& compile_desc, std::vector<unsigned char> &spirv)
	{
		STARTUPINFO startup_info{};
		PROCESS_INFORMATION process_information{};

		std::string output_spv_file = compile_desc.file_path;
		output_spv_file = output_spv_file.replace(compile_desc.file_path.find(".glsl"), 5, ".spv");
		(void)std::remove(output_spv_file.c_str());
		
		std::string shader_stage;
		switch (compile_desc.shader_type) {
		case glTFShaderUtils::ShaderType::ST_Vertex:
			shader_stage = "vert";
			break;
		case glTFShaderUtils::ShaderType::ST_Fragment:
			shader_stage = "frag";
			break;
		case glTFShaderUtils::ShaderType::ST_Compute:
			shader_stage = "comp";
			break;
		case glTFShaderUtils::ShaderType::ST_Undefined:
			shader_stage = "";
			break;
		}
		
		wchar_t executable_path[MAX_PATH];
		wsprintf(executable_path, L"glslc.exe -fshader-stage=%hs %hs -o %hs", shader_stage.c_str(), compile_desc.file_path.c_str(), output_spv_file.c_str());
		
		wchar_t current_working_directory[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, current_working_directory);
		
		bool create = CreateProcess(0, executable_path, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, current_working_directory, &startup_info, &process_information);
		GLTF_CHECK(create);

		WaitForSingleObject(process_information.hProcess, 100000000);
		
		// Loading spv file into binary array
		std::ifstream spv_file(output_spv_file, std::ios::in | std::ios::binary | std::ios::ate);
		GLTF_CHECK(spv_file.is_open());
		auto size = spv_file.tellg();
		spirv.resize(size);
		spv_file.seekg(0, std::ios::beg);
		spv_file.read((char*)spirv.data(), spirv.size());
		spv_file.close();
		
		return true;
	}
};

bool CompileShaderWithGLSL2SPIRV(const glTFShaderUtils::ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries)
{
	return SpirvHelper::GLSLtoSPVExternal(compile_desc, out_binaries);
}

bool glTFShaderUtils::CompileShader(const ShaderCompileDesc& compile_desc, std::vector<unsigned char>& out_binaries )
{
    if (compile_desc.file_path.empty())
    {
        GLTF_CHECK(false);
        return false;
    }

    bool result = false;
    switch (compile_desc.file_type)
    {
    case ShaderFileType::SFT_HLSL:
        result = CompileShaderWithDXC(compile_desc, out_binaries);
        break;
    case ShaderFileType::SFT_GLSL:
        GLTF_CHECK(compile_desc.spirv);
        result = CompileShaderWithGLSL2SPIRV(compile_desc, out_binaries);
        break;
    }
    
    return result;
}
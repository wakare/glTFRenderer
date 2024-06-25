#pragma once

#include <vector>
#include <string>
#include <cassert>
#include <codecvt>
#include <locale>

#ifdef NDEBUG
#define GLTF_CHECK(a) if (!(a)) {throw "ASSERT!"; }
#else
#define GLTF_CHECK(a) assert(a)
#endif

#define LOG_OUTPUT_STDOUT

#ifdef LOG_OUTPUT_STDOUT

#include <cstdio>
#define LOG_FLUSH() fflush(stdout);
#define LOG_FORMAT(...) printf(__VA_ARGS__); fflush(stdout);
#define LOG_FORMAT_FLUSH(...) LOG_FORMAT(__VA_ARGS__) LOG_FLUSH();

#else

class glTFLog
{
public:
    static void Log(const char* format, ...)
    {
        static_assert(false); // Not implement!
    } 
};
#define LogFormat(format, ...) glTFLog::Log(format, __VA_ARGS__);
#endif
#define THROW_IF_FAILED(x) \
{ \
HRESULT result = (x); \
if (FAILED(result)) \
{ \
LOG_FORMAT_FLUSH("[ERROR] Failed with error code: %u\n", result); \
throw std::runtime_error("[ERROR] Detected runtime error in call"); \
} \
}

#define RETURN_IF_FALSE(x) \
if (!(x)) \
{ \
assert(false); return false; \
}

inline std::wstring to_wide_string(const std::string& input)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(input);
}

enum class RHIShaderType
{
    Vertex,
    Pixel,
    Compute,
    RayTracing,
    Unknown,
};

struct RHIShaderPreDefineMacros
{
    void AddMacro(const std::string& key, const std::string& value)
    {
        macroKey.push_back(key);
        macroValue.push_back(value);
    }

    void AddCBVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(b%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddSRVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(t%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    void AddUAVRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(u%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }

    void AddSamplerRegisterDefine(const std::string& key, unsigned register_index, unsigned space = 0)
    {
        char format_string[32] = {'\0'};
        (void)snprintf(format_string, sizeof(format_string), "register(s%u, space%u)", register_index, space);
        AddMacro(key, format_string);
    }
    
    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

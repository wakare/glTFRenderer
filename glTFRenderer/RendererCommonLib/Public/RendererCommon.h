#pragma once

#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <cassert>
#include <fstream>
#include <functional>
#include <string>
#include <locale>

#include "SceneFileLoader/glTFImageIOUtil.h"

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

inline std::wstring to_wide_string(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

inline std::string to_byte_string(std::wstring_view w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
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

    std::vector<std::string> macroKey;
    std::vector<std::string> macroValue;
};

typedef unsigned RendererUniqueObjectID;
#define RendererUniqueObjectIDInvalid UINT_MAX   

#ifdef NDEBUG
#define GLTF_CHECK(a) if (!(a)) {throw "ASSERT!"; }
#else
#define GLTF_CHECK(a) assert(a)
#endif

#define IMPL_NON_COPYABLE(ClassName)\
    ClassName(const ClassName&) = delete;\
    ClassName(ClassName&&) = delete;\
    ClassName& operator=(const ClassName&) = delete;\
    ClassName& operator=(ClassName&&) = delete;

#define IMPL_NON_COPYABLE_AND_DEFAULT_CTOR(ClassName)\
    ClassName() = default;\
    IMPL_NON_COPYABLE(ClassName)

#define IMPL_VDTOR(ClassName) \
    virtual ~ClassName() = default;

#define IMPL_NON_COPYABLE_AND_VDTOR(ClassName)\
    IMPL_NON_COPYABLE(ClassName)\
    IMPL_VDTOR(ClassName)

#define IMPL_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(ClassName)\
    IMPL_NON_COPYABLE_AND_DEFAULT_CTOR(ClassName)\
    IMPL_VDTOR(ClassName)

#define IMPL_DEFAULT_CTOR_VDTOR(ClassName) \
    ClassName() = default; \
    virtual ~ClassName() = default;


//#define ALIGN_FOR_CBV_STRUCT __declspec(align(16))
#define ALIGN_FOR_CBV_STRUCT

template<typename T>
class RendererUniqueObjectIDBase
{
public:
    RendererUniqueObjectIDBase()
        : m_uniqueID(_innerUniqueID++) {}
    virtual ~RendererUniqueObjectIDBase() = default;
    
    RendererUniqueObjectID GetID() const { return m_uniqueID; }
    
private:
    RendererUniqueObjectID m_uniqueID;
    static RendererUniqueObjectID _innerUniqueID;
};

template<typename T>
RendererUniqueObjectID RendererUniqueObjectIDBase<T>::_innerUniqueID = 0;

class ITickable
{
public:
    virtual ~ITickable() = default;
    virtual void Tick() { assert(m_tickEnable); m_tickFunc(); }

    void SetTickFunc(std::function<void()> tickFunc) {m_tickEnable = true; m_tickFunc = std::move(tickFunc); }
    bool CanTick() const {return m_tickEnable; }
    
protected:
    std::function<void()> m_tickFunc;
    bool m_tickEnable {false};
};

class glTFDebugFileManager
{
public:
    template <typename DataType>
    static void OutputDataToFile(const char* file, const DataType* data, size_t count);
};

template <typename DataType>
void glTFDebugFileManager::OutputDataToFile(const char* file, const DataType* data, size_t count)
{
    std::ofstream out_file_stream(file);
    if (out_file_stream.bad())
    {
        return;
    }

    const DataType* offset = data;
    for (size_t i = 0; i < count; ++i)
    {
        out_file_stream << std::to_string(*offset++) << std::endl;
    }

    out_file_stream.close();
}

// Flags representation: https://dietertack.medium.com/using-bit-flags-in-c-d39ec6e30f08
template<typename FlagEnumType>
struct glTFFlagsBase
{
    void SetFlag(FlagEnumType flag) { m_flags |= flag; }
    void UnsetFlag(FlagEnumType flag) { m_flags &= ~static_cast<int>(flag); }
    bool IsFlagSet(FlagEnumType flag) const { return m_flags & flag; }
    
    void SetFlagEnable(FlagEnumType type, bool enable)
    {
        if (enable)
        {
            SetFlag(type);
        }
        else
        {
            UnsetFlag(type);
        }
    }
    
private:
    uint64_t m_flags { 0llu };
};

inline float Rand01()
{
    return static_cast<float>(rand()) / RAND_MAX;
}
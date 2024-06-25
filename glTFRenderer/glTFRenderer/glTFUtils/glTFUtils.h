#pragma once
#ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#endif

#include <cassert>
#include <fstream>
#include <functional>
#include <string>
#include <locale>

typedef unsigned glTFUniqueID;
#define glTFUniqueIDInvalid UINT_MAX   

#ifdef NDEBUG
#define GLTF_CHECK(a) if (!(a)) {throw "ASSERT!"; }
#else
#define GLTF_CHECK(a) assert(a)
#endif

#define DECLARE_NON_COPYABLE(ClassName)\
    ClassName(const ClassName&) = delete;\
    ClassName(ClassName&&) = delete;\
    ClassName& operator=(const ClassName&) = delete;\
    ClassName& operator=(ClassName&&) = delete;

#define DECLARE_NON_COPYABLE_AND_DEFAULT_CTOR_VDTOR(ClassName)\
    ClassName() = default;\
    virtual ~ClassName() = default;\
    DECLARE_NON_COPYABLE(ClassName)

//#define ALIGN_FOR_CBV_STRUCT __declspec(align(16))
#define ALIGN_FOR_CBV_STRUCT

template<typename T>
class glTFUniqueObject
{
public:
    glTFUniqueObject()
        : m_uniqueID(_innerUniqueID++) {}
    virtual ~glTFUniqueObject() = default;
    
    glTFUniqueID GetID() const { return m_uniqueID; }
    
private:
    glTFUniqueID m_uniqueID;
    static glTFUniqueID _innerUniqueID;
};

template<typename T>
glTFUniqueID glTFUniqueObject<T>::_innerUniqueID = 0;

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
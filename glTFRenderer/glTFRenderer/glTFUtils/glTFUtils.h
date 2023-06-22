#pragma once
#include <cassert>
#include <fstream>
#include <functional>
#include <string>

typedef unsigned glTFUniqueID;
#define glTFUniqueIDInvalid UINT_MAX   

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

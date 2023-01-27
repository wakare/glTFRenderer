#pragma once
#define LOG_OUTPUT_STDOUT

#ifdef LOG_OUTPUT_STDOUT

#include <iostream>
#define LOG_FORMAT(format, ...) printf(format, __VA_ARGS__);

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
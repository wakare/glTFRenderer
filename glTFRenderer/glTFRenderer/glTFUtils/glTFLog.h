#pragma once
#define LOG_OUTPUT_STDOUT

#ifdef LOG_OUTPUT_STDOUT

#include <cstdio>
#define LOG_FORMAT(...) printf(__VA_ARGS__); fflush(stdout);

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
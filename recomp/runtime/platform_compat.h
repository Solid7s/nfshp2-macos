#pragma once
// Small CRT/timing equivalents used by the shared runtime on POSIX hosts.
#ifndef _WIN32
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <strings.h>
#include <cwctype>

#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _fseeki64 fseeko
#define _ftelli64 ftello
inline int _wcsicmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && std::towlower(*a) == std::towlower(*b)) { ++a; ++b; }
    return int(std::towlower(*a)) - int(std::towlower(*b));
}
inline FILE *_wfopen(const wchar_t *path, const wchar_t *mode)
{
    const std::filesystem::path p(path);
    std::string m;
    while (*mode) m += static_cast<char>(*mode++);
    return fopen(p.c_str(), m.c_str());
}
using ULONG64 = uint64_t;
using ULONGLONG = uint64_t;
struct LARGE_INTEGER { int64_t QuadPart = 0; };
inline void QueryPerformanceCounter(LARGE_INTEGER *v)
{
    v->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void QueryPerformanceFrequency(LARGE_INTEGER *v) { v->QuadPart = 1000000000; }
inline uint64_t GetTickCount64()
{
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return v.QuadPart / 1000000;
}
#endif
